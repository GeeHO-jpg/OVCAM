# run_both_threads.py
import threading, runpy, sys, time, os

def run_recv():
    # รันสคริปต์ recv.py ตรง ๆ ในเธรดนี้ (คงรูปเดิมของไฟล์ไว้)
    runpy.run_path("recv.py", run_name="__main__")

def run_cmd():
    # รันสคริปต์ cmd_send.py ตรง ๆ ในอีกเธรด (คงรูปเดิม)
    runpy.run_path("cmd_send.py", run_name="__main__")

def main():
    t_rx  = threading.Thread(target=run_recv, name="RX", daemon=True)
    t_cmd = threading.Thread(target=run_cmd, name="CMD", daemon=True)
    t_rx.start()
    t_cmd.start()
    try:
      while t_rx.is_alive() and t_cmd.is_alive():
          time.sleep(0.2)
    except KeyboardInterrupt:
      print("\n[MAIN] stop requested (Ctrl+C)")
    finally:
      # ทั้งสองไฟล์เดิมของคุณเป็นลูปบล็อก I/O;
      # เมื่อปิดโปรแกรม ใช้ Ctrl+C หยุดได้ (เธรดเป็น daemon)
      pass

if __name__ == "__main__":
    main()
