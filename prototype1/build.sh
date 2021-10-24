#!/bin/sh
set -e

RUN=( out/example_mac_x64.exe )
ERRSTYLE="\e[1m\e[31m" # bold red
OKSTYLE="\e[2m" # dim

CMD=
case "$1" in
  -h*|--help)
    echo "usage: $0 [-watch | -help] [arg to ninja ...]" >&2
    echo "-w|-watch  Rebuild & run when source files changes" >&2
    exit 0
    ;;
  -w|--?watch) CMD=watch; shift ;;
esac

# _find_llvm attempts to find the path to llvm binaries, prioritizing PATH
_find_llvm() {
  if command -v wasm-ld >/dev/null; then
    echo $(dirname "$(command -v wasm-ld)")
    return 0
  fi
  local test_paths=()
  if (command -v clang >/dev/null); then
    CLANG_PATH=$(clang -print-search-dirs | grep programs: | awk '{print $2}' | sed 's/^=//')
    if [[ "$CLANG_PATH" != "" ]]; then
      test_paths+=( "$CLANG_PATH" )
    fi
  fi
  if [[ "$OSTYPE" == "darwin"* ]]; then
    test_paths+=( /Library/Developer/CommandLineTools/usr/bin )
  # elif [[ "$OSTYPE" == "linux"* ]]; then
  #   echo "linux"
  # elif [[ "$OSTYPE" == "cygwin" ]] || \
  #      [[ "$OSTYPE" == "msys" ]] || \
  #      [[ "$OSTYPE" == "win32" ]] || \
  #      [[ "$OSTYPE" == "win64" ]]
  # then
  #   echo "win"
  fi
  for path in ${test_paths[@]}; do
    if [[ -f "$path/wasm-ld" ]]; then
      echo "$path"
      return 0
    fi
  done
  echo "LLVM with WASM support not found in PATH. Also searched:" >&2
  for path in ${test_paths[@]}; do
    echo "  $path" >&2
  done
  echo "Please set PATH to include LLVM with WASM backend" >&2
  return 1
}

LLVM_PATH=$(_find_llvm)
export PATH=$LLVM_PATH:$PATH


if [ "$CMD" = "watch" ]; then

  trap exit SIGINT  # make sure we can ctrl-c in the while loop
  command -v fswatch >/dev/null || # must have fswatch
    { echo "fswatch not found in PATH" >&2 ; exit 1; }

  while true; do
    echo -e "\x1bc"  # clear screen ("scroll to top" style)
    if ninja "$@"; then
      ls -lh out/example*.*
      set +e
      printf "\e[2m> run ${RUN[@]}\e[m\n"
      printf "\e[1m" # bold command output
      "${RUN[@]}"
      status=$?
      printf "\e[0m"
      style=$OKSTYLE
      [ $status -eq 0 ] || style=$ERRSTYLE
      printf "${style}> $(basename ${RUN[0]}) exited $status\e[m\n"
      set -e
    fi
    printf "\e[2m> watching files for changes...\e[m\n"
    fswatch --one-event --extended --latency=0.1 \
            --exclude='.*' --include='\.(c|h|syms|ninja)$' .
  done
else
  exec ninja "$@"
fi
