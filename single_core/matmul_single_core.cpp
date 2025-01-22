// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "tt_metal/host_api.hpp"
#include "tt_metal/common/constants.hpp"
#include "tt_metal/detail/util.hpp"
#include "tt_metal/common/bfloat16.hpp"
#include "tt_metal/common/test_tiles.hpp"
#include "tt_metal/impl/dispatch/command_queue.hpp"
#include "tt_metal/programming_examples/matmul_common/bmm_op.hpp"
#include "tt_metal/common/tilize_untilize.hpp"
#include "impl/device/device.hpp"

#include <chrono>

using namespace tt::constants;
using namespace std;
using namespace tt;
using namespace tt::tt_metal;


void matmul_single_core(
    std::vector<bfloat16>& a,
    std::vector<bfloat16>& b,
    std::vector<bfloat16>& output,
    bool bcast_batch,
    uint32_t M,
    uint32_t N,
    uint32_t K,
    uint32_t B,
    Device* device) {
    /*
     * Setup program to execute along with its buffers and kernels to use
     * Core range is just single core
     */
    CommandQueue& cq = device->command_queue();
    Program program{};
    CoreRange core({0, 0}, {0, 0});

    /*
     * EXtracting Matrix dimensions from input/output vectors
     */
    // C = A*B
    // MN = MK*KN
    uint32_t Mt = M / TILE_HEIGHT;
    uint32_t Kt = K / TILE_WIDTH;
    uint32_t Nt = N / TILE_WIDTH;

    /*
     * Create DRAM Buffers for input and output vectors
     * Writing data from input vectors to source buffers
     */
    tt::DataFormat cb_data_format = tt::DataFormat::Float16_b;
    MathFidelity math_fidelity = MathFidelity::HiFi4;
    uint32_t single_tile_size = 2 * 32 * 32;

    uint32_t dram_buffer_A_size =
        single_tile_size * Mt * Kt;  // num_tiles of FP16_B, hard-coded in the reader/writer kernels
    uint32_t dram_buffer_B_size =
        single_tile_size * Nt * Kt;  // num_tiles of FP16_B, hard-coded in the reader/writer kernels
    uint32_t dram_buffer_C_size =
        single_tile_size * Mt * Nt;  // num_tiles of FP16_B, hard-coded in the reader/writer kernels

    /* DRAM buffer size = input full size */
    /* limiting page_size = single tile size; to allow DRAM channels interleaving */

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

    /*
     * Config of Circular Buffer in the device L1
     * input tiles count is = 2 because it's single tile process, and double-buffer
     */
    uint32_t src0_cb_index = CBIndex::c_0;  // 0
    uint32_t num_input_tiles = 2;
    CircularBufferConfig cb_src0_config =
        CircularBufferConfig(num_input_tiles * single_tile_size, {{src0_cb_index, cb_data_format}})
            .set_page_size(src0_cb_index, single_tile_size);
    auto cb_src0 = tt_metal::CreateCircularBuffer(program, core, cb_src0_config);

    uint32_t src1_cb_index = CBIndex::c_1;  // 1
    CircularBufferConfig cb_src1_config =
        CircularBufferConfig(num_input_tiles * single_tile_size, {{src1_cb_index, cb_data_format}})
            .set_page_size(src1_cb_index, single_tile_size);
    auto cb_src1 = tt_metal::CreateCircularBuffer(program, core, cb_src1_config);

    uint32_t output_cb_index = tt::CBIndex::c_16;
    uint32_t num_output_tiles = 2;
    CircularBufferConfig cb_output_config =
        CircularBufferConfig(num_output_tiles * single_tile_size, {{output_cb_index, cb_data_format}})
            .set_page_size(output_cb_index, single_tile_size);
    auto cb_output = tt_metal::CreateCircularBuffer(program, core, cb_output_config);

    /*
     * Compile time arguments
     */
    bool src0_is_dram = src0_dram_buffer->buffer_type() == tt_metal::BufferType::DRAM ? 1 : 0;
    bool src1_is_dram = src1_dram_buffer->buffer_type() == tt_metal::BufferType::DRAM ? 1 : 0;
    std::vector<uint32_t> reader_compile_time_args = {(uint32_t)src0_is_dram, (uint32_t)src1_is_dram};

    bool dst_is_dram = dst_dram_buffer->buffer_type() == tt_metal::BufferType::DRAM ? 1 : 0;
    std::vector<uint32_t> writer_compile_time_args = {(uint32_t)dst_is_dram};

    /*
     * Create Kernels (Reader, Writer, Compute)
     */
    auto reader_id = tt_metal::CreateKernel(
        program,
        "tt_metal/programming_examples/matmul_common/kernels/dataflow/reader_bmm_8bank.cpp",
        core,
        tt_metal::DataMovementConfig{
            .processor = DataMovementProcessor::RISCV_1,
            .noc = NOC::RISCV_1_default,
            .compile_args = reader_compile_time_args});

    auto writer_id = tt_metal::CreateKernel(
        program,
        "tt_metal/programming_examples/matmul_common/kernels/dataflow/writer_bmm_8bank.cpp",
        core,
        tt_metal::DataMovementConfig{
            .processor = DataMovementProcessor::RISCV_0,
            .noc = NOC::RISCV_0_default,
            .compile_args = writer_compile_time_args});

    std::vector<uint32_t> compute_args = {
        B,   // B
        Mt,  // Mt
        Kt,  // Kt
        Nt   // Nt
    };
    auto matmul_single_core_kernel_id = tt_metal::CreateKernel(
        program,
        "tt_metal/programming_examples/matmul_common/kernels/compute/bmm.cpp",
        core,
        tt_metal::ComputeConfig{.math_fidelity = math_fidelity, .compile_args = compute_args});

    /*
     * Kernels - Runtime arguments
     */
    tt_metal::SetRuntimeArgs(
        program,
        reader_id,
        core,
        {src0_addr, src1_addr, Mt, Kt, Nt, Mt * Kt, Kt * Nt, B, uint32_t(bcast_batch ? 1 : 0)});

    tt_metal::SetRuntimeArgs(program, writer_id, core, {dst_addr, 0, Mt, Kt, Nt, Mt * Kt, Kt * Nt, B});

    /* Launch program & read in output buffer result into the host vector */
    EnqueueWriteBuffer(cq, src0_dram_buffer, a.data(), false);
    EnqueueWriteBuffer(cq, src1_dram_buffer, b.data(), false);
    EnqueueProgram(cq, program, false);
    EnqueueReadBuffer(cq, dst_dram_buffer, output.data(), true);
}

///////////////////////////////////////

int main(int argc, char** argv) {
    bool pass = true;

    if (getenv("TT_METAL_SLOW_DISPATCH_MODE") != nullptr) {
        TT_THROW("Test not supported w/ slow dispatch, exiting");
    }

    try {
        using std::chrono::high_resolution_clock;
        using std::chrono::duration_cast;
        using std::chrono::duration;
        using std::chrono::milliseconds;


        /* Silicon accelerator setup */
        constexpr int device_id = 0;
        Device* device = CreateDevice(device_id);

        /* Create source data */
        constexpr uint32_t M = 1024;  // user-defined
        constexpr uint32_t N = 1024;  // user-defined
        constexpr uint32_t K = 1024;  // user-defined
        constexpr uint32_t B = 1;    // user-defined

        uint32_t Mt = M / TILE_HEIGHT;
        uint32_t Kt = K / TILE_WIDTH;
        uint32_t Nt = N / TILE_WIDTH;

        constexpr uint32_t single_tile_size = 2 * 1024;
        uint32_t dram_buffer_A_size = single_tile_size * Mt * Kt;  // num_tiles of FP16_B
        uint32_t dram_buffer_B_size = single_tile_size * Nt * Kt;  // num_tiles of FP16_B
        uint32_t dram_buffer_C_size = single_tile_size * Mt * Nt;  // num_tiles of FP16_B

        /* input vectors with various ranges of values */
        std::vector<bfloat16> src0_vec = create_random_vector_of_bfloat16_native(dram_buffer_A_size, 1, 123);
        std::vector<bfloat16> src1_vec = create_random_vector_of_bfloat16_native(dram_buffer_B_size, 1, 12522);

        /* Getting number of milliseconds as a double. */
        duration<double, std::milli> ms_double_cpu = t2 - t1;
        log_info(tt::LogVerif, "Time matmul cpu: {} ms", ms_double_cpu.count());

        /* Input vector tilizing */
        tilize(src0_vec, M, K);
        tilize(src1_vec, K, N);

        /* Calling the MatMul host program. Read in result into a host vector */
        std::vector<bfloat16> result_vec(dram_buffer_C_size / sizeof(bfloat16));

        t1 = high_resolution_clock::now();
        matmul_single_core(src0_vec, src1_vec, result_vec, false, M, N, K, B, device);
        t2 = high_resolution_clock::now();

        /* Getting number of milliseconds as a double. */
        duration<double, std::milli> ms_double_tt = t2 - t1;
        log_info(tt::LogVerif, "Time matmul tt: {} ms", ms_double_tt.count());
        
        untilize(result_vec, M, N);

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
