/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_GPU_GPU_EXECUTABLE_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_GPU_GPU_EXECUTABLE_H_

#include <memory>
#include <string>

#include "tensorflow/compiler/xla/service/buffer_assignment.h"
#include "tensorflow/compiler/xla/service/device_memory_allocator.h"
#include "tensorflow/compiler/xla/service/executable.h"
#include "tensorflow/compiler/xla/service/gpu/buffer_allocations.h"
#include "tensorflow/compiler/xla/service/gpu/stream_assignment.h"
#include "tensorflow/compiler/xla/service/gpu/temp_buffer_offsets.h"
#include "tensorflow/compiler/xla/service/gpu/thunk.h"
#include "tensorflow/compiler/xla/service/gpu/thunk_schedule.h"
#include "tensorflow/compiler/xla/service/hlo_execution_profile.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_module_config.h"
#include "tensorflow/compiler/xla/service/shaped_buffer.h"
#include "tensorflow/compiler/xla/service/tuple_points_to_analysis.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/core/lib/core/stringpiece.h"
#include "tensorflow/core/lib/gtl/array_slice.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"

namespace xla {
namespace gpu {

// GPU-targeting implementation of the XLA Executable interface.
//
// Launches the given CUDA kernel via the StreamExecutor.
//
// This is an immutable data type after initialization, and thus thread safe.
class GpuExecutable : public Executable {
 public:
  GpuExecutable(tensorflow::StringPiece ptx,
                std::unique_ptr<ThunkSchedule> thunk_schedule,
                std::unique_ptr<HloModule> hlo_module,
                std::unique_ptr<HloModuleConfig> module_config,
                std::unique_ptr<BufferAssignment> assignment,
                std::unique_ptr<TempBufferOffsets> temp_buffer_offsets);

  // This should be called after set_ir_module_string.
  const string& ir_module_string() const { return ir_module_string_; }

  // This should be called before ExecuteOnStream.
  void set_ir_module_string(const string& ir_module_string) {
    ir_module_string_ = ir_module_string;
  }

  // Returns the compiled PTX for the computation.
  tensorflow::StringPiece ptx() const { return ptx_; }

  StatusOr<perftools::gputools::DeviceMemoryBase> ExecuteOnStream(
      const ExecutableRunOptions* run_options,
      tensorflow::gtl::ArraySlice<perftools::gputools::DeviceMemoryBase>
          arguments,
      HloExecutionProfile* hlo_execution_profile) override;

  StatusOr<std::unique_ptr<ShapedBuffer>> ExecuteOnStream(
      const ExecutableRunOptions* run_options,
      tensorflow::gtl::ArraySlice<const ShapedBuffer*> arguments,
      HloExecutionProfile* hlo_execution_profile) override;

  Status ExecuteOnStream(
      const ExecutableRunOptions* run_options,
      tensorflow::gtl::ArraySlice<const ShapedBuffer*> arguments,
      ShapedBuffer* result_buffer,
      HloExecutionProfile* hlo_execution_profile) override;

  StatusOr<perftools::gputools::DeviceMemoryBase> ExecuteAsyncOnStream(
      const ExecutableRunOptions* run_options,
      tensorflow::gtl::ArraySlice<perftools::gputools::DeviceMemoryBase>
          arguments) override;

 private:
  Status ExecuteThunks(perftools::gputools::Stream* stream,
                       const BufferAllocations& buffer_allocations,
                       HloExecutionProfile* hlo_execution_profile);

  // Returns the points-to set of the root instruction of the entry
  // computation. Uses points-to analysis from buffer assignment.
  const PointsToSet& GetRootPointsToSet() const;

  // The LLVM IR, in string format, of the unoptimized module generated for this
  // GpuExecutable. We save a string instead of an llvm::Module* because leaving
  // llvm::Module* in a singleton can cause the heap checker to emit false
  // positives.
  //
  // This string should be modified only before ExecuteOnStream.
  string ir_module_string_;

  // The reference to the compiled PTX for the computation.
  const tensorflow::StringPiece ptx_;

  // The thunks to be invoked by this GpuExecutable. They are generated by the
  // IrEmitter.
  const std::unique_ptr<ThunkSchedule> thunk_schedule_;

  // Owns the buffer data at runtime. It provides information to allocate
  // memory for every output/temp buffers.
  const std::unique_ptr<BufferAssignment> assignment_;

  // Owns the mapping from temporary buffers to their offsets in the temp-buffer
  // memory block.
  const std::unique_ptr<TempBufferOffsets> temp_buffer_offsets_;

  TF_DISALLOW_COPY_AND_ASSIGN(GpuExecutable);
};

}  // namespace gpu
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_GPU_GPU_EXECUTABLE_H_
