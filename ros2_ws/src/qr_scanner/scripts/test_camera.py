import subprocess
import os
import time
import getpass
from PIL import Image
from pyzbar.pyzbar import decode
import sys

# 解决中文编码问题（Windows/Linux 通用）
def setup_chinese_encoding():
    if sys.platform.startswith('win'):
        sys.stdout.reconfigure(encoding='utf-8')
    else:
        import locale
        locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')  # Linux 强制 UTF-8 编码

def scan_qrcode(img_path):
    """
    扫描图片中的二维码，返回解码结果列表
    参数: img_path: 图片路径
    返回: 成功返回 [(二维码内容, 二维码类型), ...]，失败返回空列表
    """
    try:
        # 打开图片（支持 JPG/PNG 等格式）
        with Image.open(img_path) as img:
            # 解码二维码（支持多个二维码同时识别）
            decoded_objects = decode(img)
            results = []
            for obj in decoded_objects:
                # 解码内容（处理中文编码）
                data = obj.data.decode('utf-8', errors='replace')
                # 二维码类型（如 QRCODE、CODE128 等）
                type_ = obj.type
                results.append((data, type_))
            return results
    except FileNotFoundError:
        print(f"❌ 扫描失败：图片 {img_path} 不存在")
        return []
    except Exception as e:
        print(f"❌ 二维码扫描异常：{str(e)}")
        return []

def continuous_screenshot(save_dir="screenshots", max_count=10, interval=1):
    # 初始化中文编码
    setup_chinese_encoding()
    
    # 1. 检查是否以 root 权限运行（sudo）
    if getpass.getuser() != "root":
        print("⚠️  警告：未使用 root 权限运行，可能导致截图失败！")
        print("建议使用命令：sudo python3 continuous_capture_qr.py\n")

    # 2. 检查依赖库是否安装
    try:
        from PIL import Image
        from pyzbar.pyzbar import decode
    except ImportError:
        print("❌ 缺少二维码扫描依赖！请先执行以下命令安装：")
        print("sudo apt install -y libzbar0 libzbar-dev")
        print("pip3 install pyzbar pillow")
        sys.exit(1)

    # 3. 创建保存目录（root 权限下确保目录可写）
    os.makedirs(save_dir, exist_ok=True)
    os.chmod(save_dir, 0o777)  # 所有用户可读写
    print(f"📁 截图保存目录：{os.path.abspath(save_dir)}")
    print(f"⚙️  配置：最大保留 {max_count} 张图片，截图间隔 {interval} 秒")
    print("📷 程序运行中... 按 Ctrl+C 停止")
    print("-" * 50 + "\n")

    file_index = 0
    try:
        while True:
            # 生成图片路径（循环覆盖）
            img_name = f"photo_{file_index % max_count}.jpg"
            img_path = os.path.join(save_dir, img_name)

            # 4. 调用 rpicam-still 截图（旧版兼容，无 --quiet）
            try:
                subprocess.run(
                    ["rpicam-still", "-o", img_path, "-n","--timeout", "500"],
                    check=True,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    shell=False
                )
                print(f"✅ 截图成功：{img_name}")
            except subprocess.CalledProcessError as e:
                print(f"❌ 截图失败！返回码：{e.returncode}")
                file_index += 1
                time.sleep(interval)
                continue
            except FileNotFoundError:
                print("❌ 未找到 rpicam-still 命令！请执行 sudo apt install rpicam-apps")
                sys.exit(1)

            # 5. 扫描图片中的二维码
            qr_results = scan_qrcode(img_path)
            if qr_results:
                print("📌 二维码扫描结果：")
                for i, (data, type_) in enumerate(qr_results, 1):
                    print(f"   二维码 {i}（类型：{type_}）：{data}")
            else:
                print("❌ 未识别到二维码或图片中无二维码")

            # 索引自增 + 间隔等待
            file_index += 1
            print("-" * 50)
            time.sleep(interval)

    except KeyboardInterrupt:
        print("\n📤 程序已停止")
    except Exception as e:
        print(f"\n❌ 程序异常：{str(e)}")

if __name__ == "__main__":
    # 可自定义参数：保存目录、最大图片数、截图间隔
    continuous_screenshot(
        save_dir="screenshots",
        max_count=10,
        interval=1  # 建议根据实际需求调整（如 2 秒/次，避免扫描过于频繁）
    )
