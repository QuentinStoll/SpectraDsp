#!/usr/bin/env bash
# ================================================
#  Spectra - compilation script (Linux and macOS)
#  Epitech Paris - EIP 2025-2026
# ================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
CACHE_DIR="$SCRIPT_DIR/.cache"
BUILD_PRESET="default"

set -e
mkdir -p "$BUILD_DIR"
mkdir -p "$CACHE_DIR"


usage() {
	echo "Usage: install.sh ACTION [PRESET]"
	echo "ACTIONS"
	echo "  all				config + build (default)"
	echo "  config			cmake config only"
	echo "  build			cmake build only"
	echo "  test      		run automated tests"
	echo "  test --coverage	run automated tests and show coverage"
	echo "  re				cache delete + remake"
	echo "PRESETS"
	echo "  default			recommended (for dev or use)"
	echo "  release			with standard release features"
	echo "  all-formats		builds plugin in all availlable formats"
	echo "  dev				most debug features"
	echo "  debug			all debug features + performance profiling"
}


case "$2" in
"" )
	BUILD_PRESET="default"
	;;
default )
	BUILD_PRESET="default"
	;;
release )
	BUILD_PRESET="release"
	;;
all-formats )
	BUILD_PRESET="all-formats"
	;;
dev )
	BUILD_PRESET="dev"
	;;
debug )
	BUILD_PRESET="debug"
	;;
--coverage )
	if [ "$1" != "test" ]; then usage; exit 1; fi
	;;
-h|--help )
	usage
	exit 0
	;;
* )
	echo "[ERROR] Unknown option: $2"
	usage
	exit 1
	;;
esac


BUILD_CONFIG="Release"
if [ "$BUILD_PRESET" = "debug" ]; then BUILD_CONFIG="Debug"; fi

config() {
	echo "[INFO] Configuring cmake"
	cmake -DPROFILER_ENABLE_COVERAGE=OFF -DCMAKE_BUILD_TYPE="$BUILD_CONFIG" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DPRESET_NAME="$BUILD_PRESET"
	echo "[OK] cmake configured"
}

coverage_is_enabled() {
	[ -f "$BUILD_DIR/CMakeCache.txt" ] && grep -q "PROFILER_ENABLE_COVERAGE:BOOL=ON" "$BUILD_DIR/CMakeCache.txt"
}

ensure_normal_config() {
	if [ ! -f "$BUILD_DIR/cmake_install.cmake" ] || [ ! -f "$BUILD_DIR/CMakeCache.txt" ] || coverage_is_enabled || ! grep -q "^PRESET_NAME:.*=$BUILD_PRESET$" "$BUILD_DIR/CMakeCache.txt"; then
		config
	fi
}

ensure_coverage_config() {
	if [ ! -f "$BUILD_DIR/cmake_install.cmake" ] || [ ! -f "$BUILD_DIR/CMakeCache.txt" ] || ! coverage_is_enabled; then
		echo "[INFO] Configuring coverage"
		cmake -DPROFILER_BUILD_TESTS=ON -DPROFILER_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S "$SCRIPT_DIR" -B "$BUILD_DIR"
	fi
}

build() {
	ensure_normal_config
	echo "[INFO] Building project"
	cmake --build "$BUILD_DIR" --config "$BUILD_CONFIG" --parallel 8
	echo "[OK] Done building project"
}

test_project() {
	ensure_normal_config
	echo "[INFO] Building automated tests"
	cmake --build "$BUILD_DIR" --target ProfilerTests --config "$BUILD_CONFIG"
	echo "[INFO] Running automated tests"
	ctest --test-dir "$BUILD_DIR" -C "$BUILD_CONFIG" --output-on-failure
	echo "[OK] Tests passed"
}

test_project_with_coverage() {
	ensure_coverage_config
	echo "[INFO] Running automated tests with coverage"
	cmake --build "$BUILD_DIR" --target ProfilerCoverage
	echo "[OK] Coverage report: $BUILD_DIR/coverage/html/index.html"
}

case "$1" in
	""|all )
		config
		build
		;;
	config )
		config
		;;
	build )
		build
		;;
	test )
		if [ "$2" = "--coverage" ]; then
			test_project_with_coverage
		else
			test_project
		fi
		;;
	re )
		rm -fr -- "$BUILD_DIR"
		rm -fr -- "$CACHE_DIR"
		build
		;;
	-h|--help )
		usage
		;;
	* )
		echo "[ERROR] Unknown option: $1"
		usage
		exit 1
		;;
esac
