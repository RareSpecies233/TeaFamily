#!/usr/bin/env bash

tea_join_by() {
  local sep="$1"
  shift || true
  local out=""
  local item=""
  for item in "$@"; do
    if [[ -z "$item" ]]; then
      continue
    fi
    if [[ -z "$out" ]]; then
      out="$item"
    else
      out+="$sep$item"
    fi
  done
  printf '%s' "$out"
}

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

tea_detect_sysroot_from_compiler() {
  local compiler="$1"
  local detected=""

  detected="$("$compiler" --print-sysroot 2>/dev/null || true)"
  if [[ -n "$detected" && -d "$detected" ]]; then
    printf '%s' "$detected"
    return 0
  fi

  return 1
}

tea_resolve_sysroot() {
  local compiler="$1"
  local user_value="$2"

  if [[ -n "$user_value" ]]; then
    printf '%s' "$user_value"
    return 0
  fi

  if tea_detect_sysroot_from_compiler "$compiler"; then
    return 0
  fi

  return 1
}

tea_has_openssl_in_root() {
  local root="$1"
  if [[ -z "$root" || ! -d "$root" ]]; then
    return 1
  fi

  local has_header=0
  local has_ssl=0
  local has_crypto=0

  [[ -f "$root/usr/include/openssl/ssl.h" || -f "$root/include/openssl/ssl.h" ]] && has_header=1
  [[ -f "$root/usr/lib/libssl.a" || -f "$root/usr/lib/libssl.so" || -f "$root/lib/libssl.a" || -f "$root/lib/libssl.so" || -f "$root/lib64/libssl.a" || -f "$root/lib64/libssl.so" ]] && has_ssl=1
  [[ -f "$root/usr/lib/libcrypto.a" || -f "$root/usr/lib/libcrypto.so" || -f "$root/lib/libcrypto.a" || -f "$root/lib/libcrypto.so" || -f "$root/lib64/libcrypto.a" || -f "$root/lib64/libcrypto.so" ]] && has_crypto=1

  [[ "$has_header" -eq 1 && "$has_ssl" -eq 1 && "$has_crypto" -eq 1 ]]
}

tea_has_brotli_in_root() {
  local root="$1"
  if [[ -z "$root" || ! -d "$root" ]]; then
    return 1
  fi

  local has_header=0
  local has_common=0
  local has_dec=0
  local has_enc=0

  [[ -f "$root/usr/include/brotli/encode.h" || -f "$root/include/brotli/encode.h" ]] && has_header=1
  [[ -f "$root/usr/lib/libbrotlicommon.a" || -f "$root/usr/lib/libbrotlicommon.so" || -f "$root/lib/libbrotlicommon.a" || -f "$root/lib/libbrotlicommon.so" || -f "$root/lib64/libbrotlicommon.a" || -f "$root/lib64/libbrotlicommon.so" ]] && has_common=1
  [[ -f "$root/usr/lib/libbrotlidec.a" || -f "$root/usr/lib/libbrotlidec.so" || -f "$root/lib/libbrotlidec.a" || -f "$root/lib/libbrotlidec.so" || -f "$root/lib64/libbrotlidec.a" || -f "$root/lib64/libbrotlidec.so" ]] && has_dec=1
  [[ -f "$root/usr/lib/libbrotlienc.a" || -f "$root/usr/lib/libbrotlienc.so" || -f "$root/lib/libbrotlienc.a" || -f "$root/lib/libbrotlienc.so" || -f "$root/lib64/libbrotlienc.a" || -f "$root/lib64/libbrotlienc.so" ]] && has_enc=1

  [[ "$has_header" -eq 1 && "$has_common" -eq 1 && "$has_dec" -eq 1 && "$has_enc" -eq 1 ]]
}

tea_prepare_brotli_for_target() {
  local target_name="$1"
  local compiler="$2"
  local cxx_compiler="$3"
  local sysroot="$4"
  local processor="$5"

  if tea_has_brotli_in_root "$sysroot"; then
    printf '%s' ""
    return 0
  fi

  tea_require_cmd curl
  tea_require_cmd tar

  local build_root="${BUILD_ROOT:-$PWD/build-cross}"
  local deps_root="${TEA_CROSS_DEPS_ROOT:-$build_root/cross-deps}"
  local source_root="$deps_root/src"
  local build_dir="$deps_root/build/brotli-$target_name"
  local install_root="$deps_root/$target_name"
  local version="${TEA_BROTLI_VERSION:-1.1.0}"
  local archive="$source_root/brotli-$version.tar.gz"
  local src_dir="$source_root/brotli-$version"

  mkdir -p "$source_root" "$deps_root/build" "$install_root"

  if ! tea_has_brotli_in_root "$install_root"; then
    if [[ ! -f "$archive" ]]; then
      echo "[deps:$target_name] downloading brotli v$version" >&2
      curl -fL "https://github.com/google/brotli/archive/refs/tags/v$version.tar.gz" -o "$archive"
    fi

    if [[ ! -d "$src_dir" ]]; then
      tar -xzf "$archive" -C "$source_root"
    fi

    echo "[deps:$target_name] building brotli for cross target" >&2
    cmake -S "$src_dir" -B "$build_dir" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR="$processor" \
      -DCMAKE_C_COMPILER="$compiler" \
      -DCMAKE_CXX_COMPILER="$cxx_compiler" \
      -DCMAKE_SYSROOT="$sysroot" \
      -DCMAKE_INSTALL_PREFIX="$install_root" \
      -DCMAKE_FIND_ROOT_PATH="$sysroot;$install_root" \
      -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
      -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
      -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
      -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
      -DBUILD_SHARED_LIBS=OFF \
      -DBROTLI_DISABLE_TESTS=ON \
      1>&2

    cmake --build "$build_dir" --target install -j "$(tea_detect_jobs)" 1>&2
  fi

  if ! tea_has_brotli_in_root "$install_root"; then
    echo "[ERROR] brotli build succeeded but required files are missing under: $install_root" >&2
    return 1
  fi

  printf '%s' "$install_root"
}

tea_check_target_runtime_deps() {
  local target_name="$1"
  local sysroot="$2"

  if ! tea_has_openssl_in_root "$sysroot"; then
    echo "[ERROR] OpenSSL not found in target sysroot for $target_name: $sysroot" >&2
    echo "        You can provide a custom sysroot via LINUX_X64_SYSROOT or RPI5_SYSROOT." >&2
    return 1
  fi

  return 0
}

tea_collect_pkgconfig_dirs() {
  local sysroot="$1"
  local extra_root="$2"
  local dirs=()
  local roots=()

  roots+=("$sysroot")
  if [[ -n "$extra_root" ]]; then
    roots+=("$extra_root")
  fi

  local root=""
  for root in "${roots[@]}"; do
    [[ -d "$root/usr/lib/pkgconfig" ]] && dirs+=("$root/usr/lib/pkgconfig")
    [[ -d "$root/usr/lib64/pkgconfig" ]] && dirs+=("$root/usr/lib64/pkgconfig")
    [[ -d "$root/usr/share/pkgconfig" ]] && dirs+=("$root/usr/share/pkgconfig")
    [[ -d "$root/lib/pkgconfig" ]] && dirs+=("$root/lib/pkgconfig")
    [[ -d "$root/lib64/pkgconfig" ]] && dirs+=("$root/lib64/pkgconfig")
    [[ -d "$root/share/pkgconfig" ]] && dirs+=("$root/share/pkgconfig")
  done

  if [[ "${#dirs[@]}" -eq 0 ]]; then
    printf '%s' ""
    return 0
  fi

  printf '%s' "$(tea_join_by ':' "${dirs[@]}")"
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

  export TEA_LINUX_X64_SYSROOT="$(tea_resolve_sysroot "$TEA_LINUX_X64_CC" "${LINUX_X64_SYSROOT:-}" || true)"
  export TEA_RPI5_SYSROOT="$(tea_resolve_sysroot "$TEA_RPI5_CC" "${RPI5_SYSROOT:-}" || true)"

  if [[ -z "$TEA_LINUX_X64_SYSROOT" || ! -d "$TEA_LINUX_X64_SYSROOT" ]]; then
    echo "[ERROR] invalid Linux x64 sysroot: ${TEA_LINUX_X64_SYSROOT:-<empty>}" >&2
    return 1
  fi

  if [[ -z "$TEA_RPI5_SYSROOT" || ! -d "$TEA_RPI5_SYSROOT" ]]; then
    echo "[ERROR] invalid Raspberry Pi 5 sysroot: ${TEA_RPI5_SYSROOT:-<empty>}" >&2
    return 1
  fi

  tea_check_target_runtime_deps "linux-x64" "$TEA_LINUX_X64_SYSROOT"
  tea_check_target_runtime_deps "rpi5" "$TEA_RPI5_SYSROOT"

  export TEA_LINUX_X64_EXTRA_ROOT="$(tea_prepare_brotli_for_target "linux-x64" "$TEA_LINUX_X64_CC" "$TEA_LINUX_X64_CXX" "$TEA_LINUX_X64_SYSROOT" "x86_64")"
  export TEA_RPI5_EXTRA_ROOT="$(tea_prepare_brotli_for_target "rpi5" "$TEA_RPI5_CC" "$TEA_RPI5_CXX" "$TEA_RPI5_SYSROOT" "aarch64")"
}

tea_write_toolchain_file() {
  local file_path="$1"
  local processor="$2"
  local c_compiler="$3"
  local cxx_compiler="$4"
  local sysroot="$5"
  local extra_root="${6:-}"
  local find_root_path="$sysroot"
  local prefix_path="$sysroot/usr;$sysroot"
  local pkgconfig_libdir=""

  if [[ -n "$extra_root" ]]; then
    find_root_path="$sysroot;$extra_root"
    prefix_path="$extra_root;$extra_root/usr;$prefix_path"
  fi

  pkgconfig_libdir="$(tea_collect_pkgconfig_dirs "$sysroot" "$extra_root")"

  cat >"$file_path" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR ${processor})
set(CMAKE_C_COMPILER ${c_compiler})
set(CMAKE_CXX_COMPILER ${cxx_compiler})
set(CMAKE_SYSROOT "${sysroot}")
set(CMAKE_FIND_ROOT_PATH "${find_root_path}")
set(CMAKE_PREFIX_PATH "${prefix_path}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_SYSROOT_DIR} "${sysroot}")
set(ENV{PKG_CONFIG_LIBDIR} "${pkgconfig_libdir}")
EOF
}

tea_configure_target() {
  local project_root="$1"
  local build_dir="$2"
  local toolchain_file="$3"

  rm -f "$build_dir/CMakeCache.txt"

  cmake -S "$project_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DHTTPLIB_REQUIRE_OPENSSL=ON \
    -DHTTPLIB_REQUIRE_BROTLI=ON \
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
