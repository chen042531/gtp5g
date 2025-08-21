#!/bin/bash

# 高压力锁竞争测试

echo "=== High Pressure Lock Competition Test ==="

# 清理
sudo rmmod lockup_test 2>/dev/null || true

# 高压力参数配置
THREADS=(4 8 16 32)           # 不同的线程数
DELAYS=(10 50 100 500)       # 不同的持锁时间
LOCK_TYPES=(0 1 2)           # 0=spin_lock, 1=spin_lock_bh, 2=lock_free

for lock_type in "${LOCK_TYPES[@]}"; do
    lock_name=""
    case $lock_type in
        0) lock_name="spin_lock" ;;
        1) lock_name="spin_lock_bh" ;;
        2) lock_name="lock_free" ;;
    esac
    
    echo "========================================="
    echo "Testing: $lock_name"
    echo "========================================="
    
    for threads in "${THREADS[@]}"; do
        for delay in "${DELAYS[@]}"; do
            echo "--- Testing $lock_name: threads=$threads, delay=${delay}us ---"
            
            # 加载模块
            if sudo insmod lockup_test.ko use_bh_lock=$lock_type num_threads=$threads work_delay_us=$delay test_duration=20; then
                echo "Module loaded, monitoring..."
                
                # 监控20秒
                start_time=$(date +%s)
                while [ -f /proc/lockup_test_stats ]; do
                    current_time=$(date +%s)
                    if [ $((current_time - start_time)) -gt 25 ]; then
                        echo "Timeout reached"
                        break
                    fi
                    
                    # 检查软锁定
                    if dmesg | tail -10 | grep -q "soft lockup"; then
                        echo "SOFT LOCKUP DETECTED!"
                        echo "Configuration: $lock_name, threads=$threads, delay=${delay}us"
                        dmesg | tail -5
                        break
                    fi
                    
                    sleep 1
                done
                
                # 收集统计
                echo "Final stats:"
                cat /proc/lockup_test_stats 2>/dev/null
                
                # 卸载
                sudo rmmod lockup_test 2>/dev/null
                
            else
                echo "Failed to load module"
            fi
            
            echo ""
            sleep 2
        done
    done
done

echo "Stress test completed!"

