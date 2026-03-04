#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import subprocess
import statistics
import matplotlib.pyplot as plt


ITER = 1000                 # 测试次数
TEST_CMD = ["./ptest"]      # 测试命令
BIN_WIDTH = 0.1             # 直方图的区间宽度
OUTLIER_K = 3.0             # IQR 方法：超出 [Q1 - k*IQR, Q3 + k*IQR] 的数据被认为是离群值


# 跑一次 ptest ，返回它输出到 stdout 的两个浮点数
def run_one_test():
    try:
        res = subprocess.run(
            TEST_CMD,
            capture_output=True,    # 捕获 stdout 和 stderr
            text=True,              # 输出字节自动解码成字符串
            timeout=30              # 30s 没跑完强行终止
        )
        
        output = res.stdout.strip()
        nums = output.split()
        if len(nums) != 2:
            return None
        return tuple(map(float, nums))
        
    except Exception:
        return None


# 用 IQR 方法过滤离群值，返回过滤后的数据列表
def filter_outliers(data, k=OUTLIER_K):
    q1, _, q3 = statistics.quantiles(data, n=4)
    iqr = q3 - q1
    lower_bound = q1 - k * iqr
    upper_bound = q3 + k * iqr
    
    filtered = [x for x in data if lower_bound <= x <= upper_bound]
    return filtered


# 画直方图，无返回值
def draw_hist(data, title, filename, color):
    plt.figure(figsize=(16, 9))
    min_val = min(data)
    max_val = max(data)
    start_bin = int(min_val / BIN_WIDTH)
    end_bin = int(max_val / BIN_WIDTH) + 1      # +1 保证不向下取整
    bins = [i * BIN_WIDTH for i in range(start_bin, end_bin + 1)]
    
    plt.hist(
        data,
        bins=bins,
        color=color,
        edgecolor='black',
        alpha=0.8       # 不透明度 80%
    )

    plt.title(title)
    plt.xlabel(f"Speedup (new&delete / MemoryPool), interval width: 0.1")
    plt.ylabel("Frequency")

    plt.grid(axis='y', linestyle='--', alpha=0.5)   # 只加水平网格线，虚线
    plt.tight_layout()
    
    plt.savefig(filename, dpi=150)
    plt.close()         # 必须关闭，释放内存


# ==================== 主函数 ====================
def main():
    single_raw = []                 # 记录单对象测试数据
    batch_raw = []                  # 记录批量测试数据
    
    # 测试并获取原始数据
    for i in range(ITER):
        res = run_one_test()
        if res is not None:
            s, b = res              # single, batch
            single_raw.append(s)
            batch_raw.append(b)
        
        if (i + 1) % 100 == 0:
            print(f"Tests done: {i + 1}/{ITER}")
    
    print()
    print(f"[Raw data]")
    print(f"Single: {len(single_raw)} results, from {min(single_raw):.3f}x to {max(single_raw):.3f}x")
    print(f"Batch: {len(batch_raw)} results, from {min(batch_raw):.3f}x to {max(batch_raw):.3f}x")

    # 用 IQR 方法过滤离群值
    single_filtered = filter_outliers(single_raw)
    batch_filtered = filter_outliers(batch_raw)
    
    # 平均值
    single_mean = statistics.mean(single_filtered)
    batch_mean = statistics.mean(batch_filtered)
    
    # 中位数
    single_median = statistics.median(single_filtered)
    batch_median = statistics.median(batch_filtered)
    
    # 标准差
    single_sd = statistics.stdev(single_filtered) if len(single_filtered) > 1 else 0
    batch_sd = statistics.stdev(batch_filtered) if len(batch_filtered) > 1 else 0
    
    print()
    print(f"[Filtered data]")
    print(f"Single:")
    print(f"  Mean:   {single_mean:.2f}x")
    print(f"  Median: {single_median:.2f}x")
    print(f"  S.D.:   ±{single_sd:.2f}")
    print(f"Batch:")
    print(f"  Mean:   {batch_mean:.2f}x")
    print(f"  Median: {batch_median:.2f}x")
    print(f"  S.D.:   ±{batch_sd:.2f}")
    
    # 绘制直方图
    draw_hist(
        single_filtered,
        "Single tests' speedup distribution",
        "hist_single.png",
        "#2E86AB"
    )
    draw_hist(
        batch_filtered,
        "Batch tests' speedup distribution",
        "hist_batch.png",
        "#A23B72"
    )
    
    print()
    print(f"Done! Histograms saved as [./hist_single] & [./hist_batch].")


if __name__ == "__main__":
    main()
