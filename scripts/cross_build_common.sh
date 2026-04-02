#!/usr/bin/env bash

tea_detect_jobs() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
    return
  fi

  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.logicalcpu 2>/dev/null || echo 4
    return
  fi

  echo 4
}

tea_require_cmd() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "[ERROR] missing command: $cmd" >&2
    return 1
  fi
  return 0
}

tea_resolve_linux_x64_compilers() {
  local host_os
  local host_arch

  host_os="$(uname -s)"
  host_arch="$(uname -m)"

  if [[ "$host_os" == "Linux" && "$host_arch" == "x86_64" ]]; then
    export TEA_LINUX_X64_CC="${LINUX_X64_CC:-cc}"
    export TEA_LINUX_X64_CXX="${LINUX_X64_CXX:-c++}"
  else
    export TEA_LINUX_X64_CC="${LINUX_X64_CC:-x86_64-linux-gnu-gcc}"
    export TEA_LINUX_X64_CXX="${LINUX_X64_CXX:-x86_64-linux-gnu-g++}"
  fi
}

tea_resolve_rpi5_compilers() {
  export TEA_RPI5_CC="${RPI5_CC:-aarch64-linux-gnu-gcc}"
  export TEA_RPI5_CXX="${RPI5_CXX:-aarch64-linux-gnu-g++}"
}

tea_prepare_cross_env() {
  tea_resolve_linux_x64_compilers
  tea_resolve_rpi5_compilers

  tea_require_cmd cmake
  tea_require_cmd "$TEA_LINUX_X64_CC"
  tea_require_cmd "$TEA_LINUX_X64_CXX"
  tea_require_cmd "$TEA_RPI5_CC"
  tea_require_cmd "$TEA_RPI5_CXX"
}

tea_write_toolchain_file() {
  local file_path="$1"
  local processor="$2"
  local c_compiler="$3"
  local cxx_compiler="$4"
  local sysroot_env_var="$5"

  cat >"$file_path" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR ${processor})
set(CMAKE_C_COMPILER ${c_compiler})
set(CMAKE_CXX_COMPILER ${cxx_compiler})
if(DEFINED ENV{${sysroot_env_var}} AND NOT "\$ENV{${sysroot_env_var}}" STREQUAL "")
  set(CMAKE_SYSROOT "\$ENV{${sysroot_env_var}}")
endif()
EOF
}

tea_configure_target() {
  local project_root="$1"
  local build_dir="$2"
  local toolchain_file="$3"

  cmake -S "$project_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain_file"
}

tea_build_targets() {
  local build_dir="$1"
  shift
  local jobs
  jobs="$(tea_detect_jobs)"

  cmake --build "$build_dir" --target "$@" -j "$jobs"
}

tea_find_binary() {
  local build_dir="$1"
  local name="$2"

  if [[ -f "$build_dir/bin/$name" ]]; then
    printf '%s' "$build_dir/bin/$name"
    return
  fi

  local found
  found="$(find "$build_dir" -type f -name "$name" -perm -111 -print -quit 2>/dev/null || true)"
  printf '%s' "$found"
}

tea_copy_binary_or_fail() {
  local build_dir="$1"
  local binary_name="$2"
  local dest_dir="$3"

  local src
  src="$(tea_find_binary "$build_dir" "$binary_name")"

  if [[ -z "$src" || ! -f "$src" ]]; then
    echo "[ERROR] binary not found: $binary_name (build dir: $build_dir)" >&2
    return 1
  fi

  mkdir -p "$dest_dir"
  cp "$src" "$dest_dir/$binary_name"
  chmod +x "$dest_dir/$binary_name" || true
}
