from ovcam_lib.param_conf import config

def main():

    con =config()
    con.help()
    while True:
        try:
            line = input("> ")
        except (EOFError, KeyboardInterrupt):
            print(); break
        keep, result = con.handle_line(line)  
        if not keep:
            break
        if result:
            cmd = result["cmd"]
            if result["kind"] == "set":
                key   = result["key"]
                value = result["value"]
                pid   = result["param_id"]
                fmt   = result["fmt"]
                # print("DEBUG:", cmd, key, value, pid, fmt)
            elif result["kind"] in ("fs","pf"):
                name = result["name"]
                rid  = result["id"]  # id ของ fs/pf
                # >>> ใช้ cmd/name/id ต่อได้เลย


if __name__ == "__main__":
    main()