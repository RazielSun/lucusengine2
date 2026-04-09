#!/bin/bash

set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RUN_DIR="${RUN_DIR:-$ROOT/bin}"
LOG_DIR="$SCRIPT_DIR/logs"
TEST_LIST_FILE="${TEST_LIST_FILE:-$SCRIPT_DIR/tests.list}"
APP="${APP:-$ROOT/bin/app}"
TICKS="${TICKS:-10}"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="$LOG_DIR/run_tests_linux_${TIMESTAMP}.log"
LATEST_LOG="$LOG_DIR/run_tests_linux_latest.log"

mkdir -p "$LOG_DIR"

if [[ ! -x "$APP" ]]; then
    echo "[ERROR] App not found or not executable: $APP"
    exit 1
fi

if [[ ! -d "$RUN_DIR" ]]; then
    echo "[ERROR] Run directory not found: $RUN_DIR"
    exit 1
fi

if [[ ! -e "$RUN_DIR/content" ]]; then
    ln -sfn "$ROOT/content" "$RUN_DIR/content"
fi

SCRIPTS_DIR="$RUN_DIR/content/scripts"
TEST_DIR="${TEST_DIR:-$SCRIPTS_DIR/tests}"

: > "$LOG_FILE"
cp "$LOG_FILE" "$LATEST_LOG"

echo "== Lucus Tests ==" | tee -a "$LOG_FILE"
echo "ROOT:      $ROOT" | tee -a "$LOG_FILE"
echo "RUN_DIR:   $RUN_DIR" | tee -a "$LOG_FILE"
echo "APP:       $APP" | tee -a "$LOG_FILE"
echo "TEST_LIST: $TEST_LIST_FILE" | tee -a "$LOG_FILE"
echo "TEST_DIR:  $TEST_DIR" | tee -a "$LOG_FILE"
echo "TICKS:     $TICKS" | tee -a "$LOG_FILE"
echo "LOG_FILE:  $LOG_FILE" | tee -a "$LOG_FILE"
echo | tee -a "$LOG_FILE"

run_test() {
    local script_path="$1"

    echo "[RUN ] $script_path" | tee -a "$LOG_FILE"
    if "$APP" --directory "$RUN_DIR" --script "$script_path" --ticks "$TICKS" 2>&1 | tee -a "$LOG_FILE"; then
        echo "[ OK ] $script_path" | tee -a "$LOG_FILE"
        echo | tee -a "$LOG_FILE"
        return 0
    fi

    echo "[FAIL] $script_path" | tee -a "$LOG_FILE"
    echo "[FAIL] Full log: $LOG_FILE" | tee -a "$LOG_FILE"
    cp "$LOG_FILE" "$LATEST_LOG"
    exit 1
}

tests=()

if [[ -f "$TEST_LIST_FILE" ]]; then
    echo "[INFO] Loading tests from list" | tee -a "$LOG_FILE"
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ -z "$line" ]] && continue
        [[ "$line" =~ ^# ]] && continue
        tests+=("$line")
    done < "$TEST_LIST_FILE"
elif [[ -d "$TEST_DIR" ]]; then
    echo "[INFO] Discovering tests from directory" | tee -a "$LOG_FILE"
    while IFS= read -r path; do
        [[ -z "$path" ]] && continue
        if [[ "$path" != "$SCRIPTS_DIR/"* ]]; then
            echo "[ERROR] Test file is outside scripts directory: $path" | tee -a "$LOG_FILE"
            exit 1
        fi
        tests+=("${path#$SCRIPTS_DIR/}")
    done < <(find "$TEST_DIR" -type f -name '*.as' | sort)
else
    echo "[ERROR] No test source found. Checked list '$TEST_LIST_FILE' and directory '$TEST_DIR'" | tee -a "$LOG_FILE"
    exit 1
fi

if [[ ${#tests[@]} -eq 0 ]]; then
    echo "[ERROR] No tests found" | tee -a "$LOG_FILE"
    exit 1
fi

for test in "${tests[@]}"; do
    run_test "$test"
done

echo "[ OK ] All tests passed" | tee -a "$LOG_FILE"
cp "$LOG_FILE" "$LATEST_LOG"
