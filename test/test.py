import requests
import time
import os
import concurrent.futures
import sys
from pathlib import Path
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

# 服务器配置
SERVER_URL = "http://localhost:8080/convert"
TEST_DIR = Path(__file__).parent

def create_session():
    """创建一个带有重试机制的会话"""
    session = requests.Session()
    retry = Retry(
        total=3,
        backoff_factor=0.5,
        status_forcelist=[500, 502, 503, 504]
    )
    adapter = HTTPAdapter(max_retries=retry)
    session.mount('http://', adapter)
    session.mount('https://', adapter)
    return session

def test_single_conversion(srt_file):
    """测试单个文件的转换准确性"""
    print(f"测试文件: {srt_file}")
    
    try:
        with open(srt_file, 'rb') as f:
            files = {'file': (os.path.basename(srt_file), f, 'application/x-subrip')}
            session = create_session()
            response = session.post(SERVER_URL, files=files, timeout=30)
            
        if response.status_code == 200:
            print(f"✓ 成功转换: {srt_file}")
            return True
        else:
            print(f"✗ 转换失败: {srt_file}, 状态码: {response.status_code}")
            print(f"错误信息: {response.text}")
            return False
    except Exception as e:
        print(f"✗ 转换出错: {srt_file}, 错误: {str(e)}")
        return False

def test_concurrent_conversion(num_concurrent=5):
    """测试并发转换性能"""
    srt_files = list(TEST_DIR.glob('*.srt'))
    if not srt_files:
        print("未找到测试文件")
        return
    
    print(f"\n开始并发测试 ({num_concurrent} 个并发请求)")
    start_time = time.time()
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_concurrent) as executor:
        futures = [executor.submit(test_single_conversion, srt_file) for srt_file in srt_files[:num_concurrent]]
        results = []
        for future in concurrent.futures.as_completed(futures):
            try:
                result = future.result()
                results.append(result)
            except Exception as e:
                print(f"任务执行出错: {str(e)}")
                results.append(False)
    
    end_time = time.time()
    total_time = end_time - start_time
    
    success_count = sum(1 for r in results if r)
    print(f"\n并发测试结果:")
    print(f"总请求数: {num_concurrent}")
    print(f"成功请求数: {success_count}")
    print(f"失败请求数: {num_concurrent - success_count}")
    print(f"总耗时: {total_time:.2f} 秒")
    print(f"平均每个请求耗时: {total_time/num_concurrent:.2f} 秒")

def main():
    print("开始服务器测试...")
    
    # 获取命令行参数中的并发数
    num_concurrent = 3  # 默认值
    if len(sys.argv) > 1:
        try:
            num_concurrent = int(sys.argv[1])
            if num_concurrent < 1:
                print("错误：并发数必须大于0")
                sys.exit(1)
        except ValueError:
            print("错误：并发数必须是整数")
            sys.exit(1)
    
    # 测试单个文件转换
    test_file = TEST_DIR / "60.srt"
    if test_file.exists():
        print("\n=== 单个文件转换测试 ===")
        test_single_conversion(test_file)
    
    # 测试并发性能
    print("\n=== 并发性能测试 ===")
    test_concurrent_conversion(num_concurrent)

if __name__ == "__main__":
    main() 