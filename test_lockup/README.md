# 锁竞争测试程序

这个测试程序专门用于重现和分析您遇到的soft lockup问题，对比不同的锁机制的表现。

## 🎯 测试目标

1. **重现soft lockup问题** - 模拟您的gtp5g模块的锁竞争场景
2. **验证spin_lock_bh是否有效** - 对比spin_lock vs spin_lock_bh
3. **验证lock-free方案** - 证明无锁方案的优势

## 📋 测试参数

- `use_bh_lock`: 锁类型 (0=spin_lock, 1=spin_lock_bh, 2=lock_free)
- `num_threads`: 并发线程数 (模拟多CPU处理数据包)
- `work_delay_us`: 持锁时间(微秒) (模拟实际工作负载)
- `test_duration`: 测试持续时间(秒)

## 🚀 快速开始

### 1. 基本测试
```bash
# 测试spin_lock (容易出问题)
sudo insmod lockup_test.ko use_bh_lock=0 num_threads=8 work_delay_us=100 test_duration=30

# 查看统计
cat /proc/lockup_test_stats

# 卸载
sudo rmmod lockup_test
```

### 2. 对比测试
```bash
# 使用Makefile预定义的测试
make test-spinlock      # 测试普通spin_lock
make test-spinlock-bh   # 测试spin_lock_bh
make test-lockfree      # 测试lock-free方案
```

### 3. 自动化测试
```bash
# 运行完整的对比测试
./monitor_test.sh

# 高压力测试
./stress_test.sh
```

## 📊 观察指标

### 实时监控
```bash
# 实时查看统计
watch -n 1 cat /proc/lockup_test_stats

# 监控系统负载
top

# 检查soft lockup
dmesg | grep -E "(soft lockup|stuck|watchdog)"
```

### 关键指标

1. **Lock contentions**: 锁竞争次数 (越低越好)
2. **Update count**: 更新操作数量 (应该持续增长)
3. **Switch count**: 切换操作数量 (每秒约1次)
4. **系统响应**: 是否出现卡顿或soft lockup

## 🔍 预期结果

### spin_lock (use_bh_lock=0)
- **高竞争场景**: 可能出现soft lockup
- **表现**: 高lock contention，可能系统卡顿

### spin_lock_bh (use_bh_lock=1)  
- **改进效果**: 应该减少或消除soft lockup
- **表现**: 中等lock contention，系统相对稳定

### lock_free (use_bh_lock=2)
- **最佳性能**: 无锁竞争，无soft lockup风险
- **表现**: 零lock contention，高吞吐量

## ⚠️ 注意事项

1. **谨慎使用高参数**: 
   - 高线程数 + 长持锁时间可能导致系统卡死
   - 建议从小参数开始测试

2. **监控系统状态**:
   - 保持SSH连接以便紧急操作
   - 准备好强制重启的方法

3. **日志收集**:
   - 测试前清理dmesg: `sudo dmesg -c`
   - 测试后检查: `dmesg | grep lockup`

## 🛠️ 故障排除

### 模块加载失败
```bash
# 检查内核日志
dmesg | tail -20

# 确认内核版本兼容
uname -r
```

### 系统卡死
```bash
# 如果SSH响应慢，尝试:
echo t > /proc/sysrq-trigger  # 显示所有任务堆栈
echo l > /proc/sysrq-trigger  # 显示锁信息

# 强制卸载模块 (危险)
echo f > /proc/sysrq-trigger  # 内存信息
```

## 📈 测试建议

### 逐步测试策略
1. **低压力测试**: 2-4线程，50us延迟
2. **中压力测试**: 8线程，100us延迟  
3. **高压力测试**: 16+线程，200us+延迟

### 验证方法
1. **功能验证**: 检查计数器是否正确更新
2. **性能验证**: 对比不同方案的吞吐量
3. **稳定性验证**: 长时间运行是否出现soft lockup

这个测试程序应该能帮您清楚地看到不同锁机制的差异！

