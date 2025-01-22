// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "tt_metal/host_api.hpp"
#include "tt_metal/common/constants.hpp"
#include "tt_metal/detail/util.hpp"
#include "tt_metal/common/bfloat16.hpp"
#include "tt_metal/common/test_tiles.hpp"
#include "tt_metal/impl/dispatch/command_queue.hpp"
#include "tt_metal/common/work_split.hpp"
#include "tt_metal/programming_examples/matmul_common/bmm_op.hpp"
#include "tt_metal/common/tilize_untilize.hpp"
#include "tt_metal/impl/device/device.hpp"

#include <chrono>
#include <tuple>

using namespace tt::constants;
using namespace std;
using namespace tt;
using namespace tt::tt_metal;

using std::chrono::high_resolution_clock;
using std::chrono::duration;
using std::chrono::milliseconds;

constexpr uint32_t dim = 256;

/* Create source data */
constexpr uint32_t M = dim;  // user-defined
constexpr uint32_t N = dim;  // user-defined
constexpr uint32_t K = dim;  // user-defined
constexpr uint32_t B = 1;    // user-defined

duration<double, std::milli> calc_duration(
    std::chrono::time_point<std::chrono::high_resolution_clock> t1, 
    std::chrono::time_point<std::chrono::high_resolution_clock> t2, 
    string message){

    duration<double, std::milli> dur = t2 - t1;
    tt::log_info(tt::LogVerif, "T {}: {} ms", message, dur.count());
    return dur;
}

std::tuple<uint32_t, uint32_t, uint32_t, std::shared_ptr<tt::tt_metal::Buffer>, std::shared_ptr<tt::tt_metal::Buffer> , std::shared_ptr<tt::tt_metal::Buffer>> 
create_DRAM_buffers(
    Device* device,
    uint32_t single_tile_size,
    uint32_t Mt,
    uint32_t Kt,
    uint32_t Nt){
    /*
     * Create DRAM Buffers for input and output vectors
     * Writing data from input vectors to source buffers
     */

    uint32_t dram_buffer_A_size =
        single_tile_size * Mt * Kt;  // num_tiles of FP16_B, hard-coded in the reader/writer kernels
    uint32_t dram_buffer_B_size =
        single_tile_size * Nt * Kt;  // num_tiles of FP16_B, hard-coded in the reader/writer kernels
    uint32_t dram_buffer_C_size =
        single_tile_size * Mt * Nt;  // num_tiles of FP16_B, hard-coded in the reader/writer kernels

    tt_metal::InterleavedBufferConfig dram_config_A{
        .device = device,
        .size = dram_buffer_A_size,
        .page_size = single_tile_size,
        .buffer_type = tt_metal::BufferType::DRAM};

    tt_metal::InterleavedBufferConfig dram_config_B{
        .device = device,
        .size = dram_buffer_B_size,
        .page_size = single_tile_size,
        .buffer_type = tt_metal::BufferType::DRAM};

    tt_metal::InterleavedBufferConfig dram_config_C{
        .device = device,
        .size = dram_buffer_C_size,
        .page_size = single_tile_size,
        .buffer_type = tt_metal::BufferType::DRAM};

    std::shared_ptr<tt::tt_metal::Buffer> src0_dram_buffer = CreateBuffer(dram_config_A);
    std::shared_ptr<tt::tt_metal::Buffer> src1_dram_buffer = CreateBuffer(dram_config_B);
    std::shared_ptr<tt::tt_metal::Buffer> dst_dram_buffer = CreateBuffer(dram_config_C);
    uint32_t src0_addr = src0_dram_buffer->address();
    uint32_t src1_addr = src1_dram_buffer->address();
    uint32_t dst_addr = dst_dram_buffer->address();

    return std::make_tuple(src0_addr, src1_addr, dst_addr, src0_dram_buffer, src1_dram_buffer, dst_dram_buffer);
}

tuple<CBHandle, CBHandle, CBHandle> configurate_L1_CBs(
    Program& program, 
    uint32_t single_tile_size,
    DataFormat cb_data_format,
    CoreRangeSet all_cores,
    uint32_t output_cb_index
    ){
    /*
     * Config of Circular Buffer in the device L1
     * input tiles count is = 2 because it's single tile process, and double-buffer
     */
    uint32_t src0_cb_index = CBIndex::c_0;  // 0
    uint32_t num_input_tiles = 2;
    CircularBufferConfig cb_src0_config =
        CircularBufferConfig(num_input_tiles * single_tile_size, {{src0_cb_index, cb_data_format}})
            .set_page_size(src0_cb_index, single_tile_size);
    auto cb_src0 = tt_metal::CreateCircularBuffer(program, all_cores, cb_src0_config);

    uint32_t src1_cb_index = CBIndex::c_1;  // 1
    CircularBufferConfig cb_src1_config =
        CircularBufferConfig(num_input_tiles * single_tile_size, {{src1_cb_index, cb_data_format}})
            .set_page_size(src1_cb_index, single_tile_size);
    auto cb_src1 = tt_metal::CreateCircularBuffer(program, all_cores, cb_src1_config);

    uint32_t num_output_tiles = 2;
    CircularBufferConfig cb_output_config =
        CircularBufferConfig(num_output_tiles * single_tile_size, {{output_cb_index, cb_data_format}})
            .set_page_size(output_cb_index, single_tile_size);
    auto cb_output = tt_metal::CreateCircularBuffer(program, all_cores, cb_output_config);

    return std::make_tuple(cb_src0, cb_src1, cb_output);

}


tuple<KernelHandle, KernelHandle, KernelHandle, KernelHandle> create_kernels(
    Program& program, 
    std::shared_ptr<tt::tt_metal::Buffer> src0_dram_buffer,
    std::shared_ptr<tt::tt_metal::Buffer> src1_dram_buffer,
    std::shared_ptr<tt::tt_metal::Buffer> dst_dram_buffer,
    uint32_t src0_addr,
    uint32_t src1_addr,
    uint32_t dst_addr,
    uint32_t output_cb_index,
    CoreRangeSet all_cores,
    CoreRangeSet core_group_1,
    CoreRangeSet core_group_2,
    uint32_t num_output_tiles_per_core_group_1,
    uint32_t num_output_tiles_per_core_group_2,
    uint32_t Kt,
    MathFidelity math_fidelity
    ){
    
    /*
     * Compile time arguments
     */
    bool src0_is_dram = src0_dram_buffer->buffer_type() == tt_metal::BufferType::DRAM ? 1 : 0;
    bool src1_is_dram = src1_dram_buffer->buffer_type() == tt_metal::BufferType::DRAM ? 1 : 0;
    std::vector<uint32_t> reader_compile_time_args = {(uint32_t)src0_is_dram, (uint32_t)src1_is_dram};

    bool dst_is_dram = dst_dram_buffer->buffer_type() == tt_metal::BufferType::DRAM ? 1 : 0;
    std::vector<uint32_t> writer_compile_time_args = {(std::uint32_t)output_cb_index, (uint32_t)dst_is_dram};


    /*
     * Create Kernels (Reader, Writer, Compute)
     */
    auto reader_id = tt_metal::CreateKernel(
        program,
        "tt_metal/programming_examples/matmul_common/kernels/dataflow/reader_bmm_8bank_output_tiles_partitioned.cpp",
        all_cores,
        tt_metal::DataMovementConfig{
            .processor = DataMovementProcessor::RISCV_1,
            .noc = NOC::RISCV_1_default,
            .compile_args = reader_compile_time_args});

    auto writer_id = tt_metal::CreateKernel(
        program,
        "tt_metal/programming_examples/matmul_common/kernels/dataflow/writer_unary_interleaved_start_id.cpp",
        all_cores,
        tt_metal::DataMovementConfig{
            .processor = DataMovementProcessor::RISCV_0,
            .noc = NOC::RISCV_0_default,
            .compile_args = writer_compile_time_args});

    std::vector<uint32_t> compute_args_group_1 = {
        1,                                 // B
        1,                                 // Mt
        Kt,                                // Kt
        num_output_tiles_per_core_group_1  // Nt
    };  // bmm compute kernel the B, Mt, Nt are just 3 for loops that technically act as 1 large loop, so only set Nt
        // for simplicity

    auto matmul_multi_core_kernel_group_1_id = tt_metal::CreateKernel(
        program,
        "tt_metal/programming_examples/matmul_common/kernels/compute/bmm.cpp",
        core_group_1,
        tt_metal::ComputeConfig{.math_fidelity = math_fidelity, .compile_args = compute_args_group_1});


    KernelHandle matmul_multi_core_kernel_group_2_id = KernelHandle();

    if (!core_group_2.ranges().empty()) {
        std::vector<uint32_t> compute_args_group_2 = {
            1,                                 // B
            1,                                 // Mt
            Kt,                                // Kt
            num_output_tiles_per_core_group_2  // Nt
        };  // bmm compute kernel the B, Mt, Nt are just 3 for loops that technically act as 1 large loop, so only set
            // Nt for simplicity

        KernelHandle matmul_multi_core_kernel_group_2_id = tt_metal::CreateKernel(
            program,
            "tt_metal/programming_examples/matmul_common/kernels/compute/bmm.cpp",
            core_group_2,
            tt_metal::ComputeConfig{.math_fidelity = math_fidelity, .compile_args = compute_args_group_2});
    }

    return std::make_tuple(reader_id, writer_id, matmul_multi_core_kernel_group_1_id, matmul_multi_core_kernel_group_2_id);

}


void matmul_multi_core(
    std::vector<bfloat16>& a,
    std::vector<bfloat16>& b,
    std::vector<bfloat16>& output,
    bool bcast_batch,
    uint32_t M,
    uint32_t N,
    uint32_t K,  
    uint32_t B,
    tt::DataFormat cb_data_format,
    MathFidelity math_fidelity,
    Device* device) {
    
    auto t1 = high_resolution_clock::now();

    /*
     * Setup program to execute along with its buffers and kernels to use
     */
    CommandQueue& cq = device->command_queue();
    Program program{};

    /*
     * Multi-Core prep
     */
    auto compute_with_storage_grid_size = device->compute_with_storage_grid_size();
    uint32_t num_cores_x = compute_with_storage_grid_size.x;
    uint32_t num_cores_y = compute_with_storage_grid_size.y;

    // From tt_metal/common/constants.hpp
    auto num_output_tiles_total = (M * N) / TILE_HW;

    /*
     * Extracting Matrix dimensions from input/output vectors
     */
    // C = A*B
    // MN = MK*KN
    uint32_t Mt = M / TILE_HEIGHT;
    uint32_t Kt = K / TILE_WIDTH;
    uint32_t Nt = N / TILE_WIDTH;
    uint32_t KtNt = Kt * Nt;
    uint32_t MtKt = Mt * Kt;
    uint32_t MtNt = Mt * Nt;

    uint32_t single_tile_size = 2 * 32 * 32;

    auto [
        src0_addr, 
        src1_addr, 
        dst_addr, 
        src0_dram_buffer, 
        src1_dram_buffer, 
        dst_dram_buffer] = create_DRAM_buffers(device, single_tile_size, Mt, Kt, Nt);

    /*
     * Use a helper function to deduce the splits needed to co-operatively do
     * this matmul.
     */
    auto[
        num_cores,
        all_cores,
        core_group_1,
        core_group_2,
        num_output_tiles_per_core_group_1,
        num_output_tiles_per_core_group_2] = split_work_to_cores(compute_with_storage_grid_size, num_output_tiles_total);

    uint32_t output_cb_index = tt::CBIndex::c_16;
    auto[cb_src0, cb_src1, cb_output] = configurate_L1_CBs(program, single_tile_size, cb_data_format, all_cores, output_cb_index);

    auto [
        reader_id, 
        writer_id, 
        matmul_multi_core_kernel_group_1_id, 
        matmul_multi_core_kernel_group_2_id] = create_kernels(program, 
                                                                src0_dram_buffer, src1_dram_buffer, dst_dram_buffer, 
                                                                src0_addr, src1_addr, dst_addr, output_cb_index,
                                                                all_cores, core_group_1, core_group_2, num_output_tiles_per_core_group_1, num_output_tiles_per_core_group_1,
                                                                Kt, math_fidelity);

    /*
     * Kernels - Runtime arguments
     */
    for (uint32_t i = 0, num_tiles_written = 0; i < num_cores; i++) {
        CoreCoord core = {i / num_cores_y, i % num_cores_y};

        uint32_t num_output_tiles_per_core = 0;
        if (core_group_1.contains(core)) {
            num_output_tiles_per_core = num_output_tiles_per_core_group_1;
        } else if (core_group_2.contains(core)) {
            num_output_tiles_per_core = num_output_tiles_per_core_group_2;
        } else {
            TT_ASSERT(false, "Core not in specified core ranges");
        }

        tt_metal::SetRuntimeArgs(program, reader_id, core,{src0_addr, src1_addr, Mt, Kt, Nt, MtKt, KtNt,
             B, uint32_t(bcast_batch), num_tiles_written, num_output_tiles_per_core, MtNt});

        tt_metal::SetRuntimeArgs(program, writer_id, core, {dst_addr, num_output_tiles_per_core, num_tiles_written});
        
        num_tiles_written += num_output_tiles_per_core;
    }

    auto t2 = high_resolution_clock::now();
    calc_duration(t1, t2, "config");

    /* Input vector tilizing */
    t1 = high_resolution_clock::now();
    tilize(a, M, K);
    tilize(b, K, N);
    t2 = high_resolution_clock::now();
    calc_duration(t1, t2, "tilizing");

    /* Launch program & read in output buffer result into the host vector */
    t1 = high_resolution_clock::now();
    EnqueueWriteBuffer(cq, src0_dram_buffer, a.data(), false);
    EnqueueWriteBuffer(cq, src1_dram_buffer, b.data(), false);
    t2 = high_resolution_clock::now();
    calc_duration(t1, t2, "write buffer");

    t1 = high_resolution_clock::now();
    EnqueueProgram(cq, program, false);
    t2 = high_resolution_clock::now();
    calc_duration(t1, t2, "matmul");
    
    t1 = high_resolution_clock::now();
    EnqueueReadBuffer(cq, dst_dram_buffer, output.data(), true);
    t2 = high_resolution_clock::now();
    calc_duration(t1, t2, "read buffer");
    
}

void print_tensor(std::vector<bfloat16> data, Device* device){
    for (auto val : data){
        cout << val << endl;
    }
}

///////////////////////////////////////

int main(int argc, char** argv) {
    bool pass = true;

    if (getenv("TT_METAL_SLOW_DISPATCH_MODE") != nullptr) {
        TT_THROW("Test not supported w/ slow dispatch, exiting");
    }

    try {

        /* Silicon accelerator setup */
        constexpr int device_id = 0;
        Device* device = CreateDevice(device_id);

        uint32_t Mt = M / TILE_HEIGHT;
        uint32_t Kt = K / TILE_WIDTH;
        uint32_t Nt = N / TILE_WIDTH;

        constexpr uint32_t single_tile_size = 2 * 32 * 32;
        uint32_t dram_buffer_A_size = single_tile_size * Mt * Kt;  // num_tiles of FP16_B
        uint32_t dram_buffer_B_size = single_tile_size * Nt * Kt;  // num_tiles of FP16_B
        uint32_t dram_buffer_C_size = single_tile_size * Mt * Nt;  // num_tiles of FP16_B

        /* input vectors with various ranges of values */
        std::vector<bfloat16> src0_vec = create_random_vector_of_bfloat16_native(dram_buffer_A_size, 1, 123, -0.4);
        std::vector<bfloat16> src1_vec = create_random_vector_of_bfloat16_native(dram_buffer_B_size, 1, 12522, -0.2);
        

        tt::DataFormat cb_data_format = tt::DataFormat::Float16_b;
        MathFidelity math_fidelity = MathFidelity::HiFi4;

        /* Calling the MatMul host program. Read in result into a host vector */
        std::vector<bfloat16> result_vec(dram_buffer_C_size / sizeof(bfloat16));
        auto t1 = high_resolution_clock::now();
        matmul_multi_core(src0_vec, src1_vec, result_vec, false, M, N, K, B, cb_data_format, math_fidelity, device);
        auto t2 = high_resolution_clock::now();
        calc_duration(t1, t2, "tot matmul");

        untilize(result_vec, M, N);

        log_info(tt::LogVerif, "Output vector of size {}", result_vec.size());

        pass &= CloseDevice(device);

    } catch (const std::exception& e) {
        tt::log_error(tt::LogTest, "Test failed with exception!");
        tt::log_error(tt::LogTest, "{}", e.what());

        throw;
    }

    if (pass) {
        tt::log_info(tt::LogTest, "Test Passed");
    } else {
        TT_THROW("Test Failed");
    }

    TT_ASSERT(pass);

    return 0;
}
