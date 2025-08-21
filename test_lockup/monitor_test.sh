#!/bin/bash

# 锁竞争测试监控脚本

LOG_FILE="test_results_$(date +%Y%m%d_%H%M%S).log"
DMESG_LOG="dmesg_$(date +%Y%m%d_%H%M%S).log"

echo "=== Lock Competition Test Monitor ===" | tee "$LOG_FILE"
echo "Start time: $(date)" | tee -a "$LOG_FILE"
echo "Log file: $LOG_FILE"
echo "Dmesg log: $DMESG_LOG"

# 清理之前的模块
sudo rmmod lockup_test 2>/dev/null || true

# 获取初始的dmesg
dmesg > "${DMESG_LOG}.before"

# 运行测试函数
run_test() {
    local test_name="$1"
    local params="$2"
    
    echo "========================================" | tee -a "$LOG_FILE"
    echo "Running test: $test_name" | tee -a "$LOG_FILE"
    echo "Parameters: $params" | tee -a "$LOG_FILE"
    echo "Start time: $(date)" | tee -a "$LOG_FILE"
    
    # 加载模块
    echo "Loading module..." | tee -a "$LOG_FILE"
    if sudo insmod lockup_test.ko $params; then
        echo "Module loaded successfully" | tee -a "$LOG_FILE"
    else
        echo "Failed to load module" | tee -a "$LOG_FILE"
        return 1
    fi
    
    # 监控系统资源
    echo "Monitoring system..." | tee -a "$LOG_FILE"
    
    # 后台监控CPU使用率
    (
        while [ -f /proc/lockup_test_stats ]; do
            echo "=== $(date) ===" >> "cpu_${test_name}.log"
            top -bn1 | head -20 >> "cpu_${test_name}.log"
            echo "" >> "cpu_${test_name}.log"
            sleep 5
        done
    ) &
    monitor_pid=$!
    
    # 后台监控模块统计
    (
        while [ -f /proc/lockup_test_stats ]; do
            echo "=== $(date) ===" >> "stats_${test_name}.log"
            cat /proc/lockup_test_stats >> "stats_${test_name}.log"
            echo "" >> "stats_${test_name}.log"
            sleep 2
        done
    ) &
    stats_pid=$!
    
    # 后台监控软锁定
    (
        while [ -f /proc/lockup_test_stats ]; do
            dmesg | grep -E "(soft lockup|stuck|watchdog)" | tail -10 >> "lockup_${test_name}.log"
            sleep 1
        done
    ) &
    lockup_pid=$!
    
    # 显示实时统计
    echo "Press Ctrl+C to stop monitoring early..."
    trap 'echo "Monitoring interrupted"; kill $monitor_pid $stats_pid $lockup_pid 2>/dev/null; break' INT
    
    while [ -f /proc/lockup_test_stats ]; do
        clear
        echo "=== Real-time Monitor: $test_name ==="
        echo "Time: $(date)"
        echo ""
        cat /proc/lockup_test_stats 2>/dev/null || break
        echo ""
        echo "Recent dmesg (soft lockup related):"
        dmesg | grep -E "(soft lockup|stuck|watchdog)" | tail -5
        echo ""
        echo "Press Ctrl+C to stop monitoring..."
        sleep 3
    done
    
    # 停止后台进程
    kill $monitor_pid $stats_pid $lockup_pid 2>/dev/null
    
    # 收集最终统计
    echo "Final statistics:" | tee -a "$LOG_FILE"
    cat /proc/lockup_test_stats 2>/dev/null | tee -a "$LOG_FILE"
    
    # 卸载模块
    echo "Unloading module..." | tee -a "$LOG_FILE"
    sudo rmmod lockup_test
    
    # 检查是否有soft lockup
    echo "Checking for soft lockups..." | tee -a "$LOG_FILE"
    if dmesg | grep -E "(soft lockup|stuck|watchdog)" | tail -10 | grep -q lockup; then
        echo "WARNING: Soft lockup detected!" | tee -a "$LOG_FILE"
        dmesg | grep -E "(soft lockup|stuck|watchdog)" | tail -10 | tee -a "$LOG_FILE"
    else
        echo "No soft lockup detected" | tee -a "$LOG_FILE"
    fi
    
    echo "End time: $(date)" | tee -a "$LOG_FILE"
    echo "Test completed: $test_name" | tee -a "$LOG_FILE"
    echo ""
}

# 主测试序列
echo "Starting lock competition tests..."

# 测试1: 普通spin_lock (最容易出问题)
run_test "spin_lock" "use_bh_lock=0 num_threads=8 work_delay_us=100 test_duration=30"

sleep 5

# 测试2: spin_lock_bh (改进版)
run_test "spin_lock_bh" "use_bh_lock=1 num_threads=8 work_delay_us=100 test_duration=30"

sleep 5

# 测试3: lock-free (最佳版)
run_test "lock_free" "use_bh_lock=2 num_threads=8 work_delay_us=100 test_duration=30"

# 生成最终报告
echo "========================================" | tee -a "$LOG_FILE"
echo "TEST SUMMARY" | tee -a "$LOG_FILE"
echo "========================================" | tee -a "$LOG_FILE"

echo "Files generated:" | tee -a "$LOG_FILE"
ls -la *.log | tee -a "$LOG_FILE"

echo ""
echo "Test completed. Check $LOG_FILE for full results."
echo "Individual test logs: cpu_*.log, stats_*.log, lockup_*.log"

