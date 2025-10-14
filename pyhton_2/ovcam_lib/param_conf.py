class config:
    CMD_FS  = 0x50              # framesize by name   payload: [u8 len][ASCII name...]
    CMD_PF  = 0x51              # pixformat by name  payload: [u8 len][ASCII name...]
    CMD_SET = 0x52              # set param          payload: [u8 id][u8 fmt][u8 size][value...]
    CMD_V_flip = 0x53
    CMD_H_flip = 0x54

    FMTNAME = {0: "int8", 1: "uint8", 2: "int16", 3: "uint16"}

    FRAMESIZE = {
        "96X96"         :0,
        "QQVGA"         :1,
        "128X128"       :2,
        "QCIF"          :3,
        "HQVGA"         :4,
        "240X240"       :5,
        "QVGA"          :6,
        "320X320"       :7,
        "CIF"           :8,
        "HVGA"          :9,
        "VGA"           :10,
        "SVGA"          :11,
        "XGA"           :12,
        "HD"            :13,
        "SXGA"          :14,
        "UXGA"          :15,
        "FHD"           :16,
        "P_HD"          :17,
        "P_3MP"         :18,
        "QXGA"          :19,
        "QHD"           :20,
        "WQXGA"         :21,
        "P_FHD"         :22,
        "QSXGA"         :23,
        "5MP"           :24
    }
    PIXFORMAT = {
        "RGB565"        :0,
        "YUV422"        :1,
        "YUV420"        :2,
        "GRAYSCALE"     :3,
        "JPEG"          :4,
        "RGB888"        :5,
        "RAW"           :6,
        "RGB444"        :7,
        "RGB555"        :8,
        "RAW8"          :9
    }
    PARAMS = {          #key,fmt,min,max
    "framesize"     : ( 0, 1,   0,   10),  # framesize_t enum
    "scale"         : ( 1, 1,   0,    1),
    "binning"       : ( 2, 1,   0,    1),

    "quality"       : ( 3, 1,   0,   63),
    "brightness"    : ( 4, 0,  -2,    2),
    "contrast"      : ( 5, 0,  -2,    2),
    "saturation"    : ( 6, 0,  -2,    2),
    "sharpness"     : ( 7, 0,  -2,    2),
    "denoise"       : ( 8, 1,   0,  255),

    "special_effect": ( 9, 1,   0,    6),
    "wb_mode"       : (10, 1,   0,    4),
    "awb"           : (11, 1,   0,    1),
    "awb_gain"      : (12, 1,   0,    1),

    "aec"           : (13, 1,   0,    1),
    "aec2"          : (14, 1,   0,    1),
    "ae_level"      : (15, 0,  -2,    2),
    "aec_value"     : (16, 3,   0, 1200),

    "agc"           : (17, 1,   0,    1),
    "agc_gain"      : (18, 1,   0,   30),
    "gainceiling"   : (19, 1,   0,    6),

    "bpc"           : (20, 1,   0,    1),
    "wpc"           : (21, 1,   0,    1),
    "raw_gma"       : (22, 1,   0,    1),
    "lenc"          : (23, 1,   0,    1),

    "hmirror"       : (24, 1,   0,    1),
    "vflip"         : (25, 1,   0,    1),
    "dcw"           : (26, 1,   0,    1),
    "colorbar"      : (27, 1,   0,    1),
    }

        # --- helpers สั้น ๆ ---
    def _fs_id(self, name:str) -> int:
        k = name.strip().upper()
        if k not in self.FRAMESIZE: raise ValueError(f"unknown framesize: {name}")
        return self.FRAMESIZE[k]

    def _pf_id(self, name:str) -> int:
        k = name.strip().upper()
        if k not in self.PIXFORMAT: raise ValueError(f"unknown pixformat: {name}")
        return self.PIXFORMAT[k]
    # def _flip_id(self,name:str)->int:
    #     k = name.strip().upper()

    def _param(self, key:str, value:int):
        k = key.strip().lower()
        if k not in self.PARAMS: raise ValueError(f"unknown param: {key}")
        pid, fmt, vmin, vmax = self.PARAMS[k]
        v = int(value)
        if not (vmin <= v <= vmax): raise ValueError(f"{k} out of range [{vmin}..{vmax}]")
        return k, pid, fmt, v, vmin, vmax

    # --- คำสั่งหลัก ---
    def do_fs(self, name:str, *, quiet=False):
        fid = self._fs_id(name)
        res = {"cmd": self.CMD_FS, "kind":"fs", "name":name.strip().upper(), "id":fid}
        if not quiet:
            print(f"CMD=0x{self.CMD_FS:02X}  fs={res['name']}  id={fid}")
        return res

    def do_pf(self, name:str, *, quiet=False):
        pid = self._pf_id(name)
        res = {"cmd": self.CMD_PF, "kind":"pf", "name":name.strip().upper(), "id":pid}
        if not quiet:
            print(f"CMD=0x{self.CMD_PF:02X}  pf={res['name']}  id={pid}")
        return res
    
    def do_flip(self, axis: str, *, quiet=False):
        a = axis.strip().lower()
        if a not in ("h", "v"):
            raise ValueError("flip axis must be 'h' or 'v'")
        
        cmd = self.CMD_H_flip if a == "h" else self.CMD_V_flip
        res = {"cmd": cmd, "kind": "flip", "axis": a}
        if not quiet:
            print(f"CMD=0x{cmd:02X}  flip={a}")
        return res
    

    def do_set(self, key:str, value:str, *, quiet=False):
        k, pid, fmt, v, vmin, vmax = self._param(key, int(value, 0))
        res = {"cmd": self.CMD_SET, "kind":"set", "key":k, "value":v, "param_id":pid, "fmt":fmt}
        if not quiet:
            print(f"CMD=0x{self.CMD_SET:02X}  set {k}={v}  (param_id={pid}, fmt={self.FMTNAME.get(fmt,fmt)}, range={vmin}..{vmax})")
        return res
    
    def do_list(self, what: str = "params", *, quiet: bool = False):
        """พิมพ์/คืนรายการตัวเลือก: fs | pf | params"""
        w = (what or "params").strip().lower()
        d = self.FRAMESIZE if w == "fs" else self.PIXFORMAT if w == "pf" else self.PARAMS
        items = sorted(d.keys())
        if not quiet:
            print(", ".join(items))
        # เผื่ออยากเอาไปใช้ต่อใน main:
        return {"kind": "list", "what": w, "items": items}


    def help(self):
        print("command: fs <NAME>            set FRAMESIZE")
        print("command: pf <NAME>            set PIXFORMAT")
        print("command: set <KEY> <VAL>      set PARAMS")
        print("command: flip h|v             send flip command only (no payload)")
        print("command: list fs|pf|params    show options")
        print("command: q|ctrl+c             exit")

    # --- handler 1 บรรทัด ---
    
    def handle_line(self, line:str, *, quiet=False):
        line = line.strip()
        if not line: return True, None
        low  = line.lower()
        if low in ("q","quit","exit"): return False, None
        if low.startswith("help"): 
            self.help(); 
            return True, None

        # list
        if low.startswith("list"):
            parts = low.split()
            self.do_list(parts[1] if len(parts)>1 else "params")
            return True, None

        # if low.startswith("flip"):
        #     parts = low.split()
        #     if len(parts) == 2 and parts[1] in ("h", "v"):
        #         return True, self.do_flip(parts[1])
        #     print("usage: flip h|v")
        #     return True, None
        
        parts = line.split()
        cmd0  = parts[0].lower()

        try:
            # key=value
            if "=" in cmd0 and len(parts)==1:
                k, v = cmd0.split("=", 1)
                return True, self.do_set(k, v, quiet=quiet)

            # ไม่ใส่ set: "<key> <val>"
            if len(parts)==2 and cmd0 in self.PARAMS:
                return True, self.do_set(cmd0, parts[1], quiet=quiet)

            # แบบเดิม: "set <key> <val>"
            if len(parts)==3 and cmd0=="set":
                return True, self.do_set(parts[1], parts[2], quiet=quiet)

            # fs / pf
            if len(parts)==2 and cmd0=="fs":
                return True, self.do_fs(parts[1], quiet=quiet)
            if len(parts)==2 and cmd0=="pf":
                return True, self.do_pf(parts[1], quiet=quiet)
            
            # flip h|v  (ส่งเป็นคำสั่งล้วน ไม่มี payload)
            if len(parts)==2 and cmd0=="flip":
                return True, self.do_flip(parts[1], quiet=quiet)


            print("syntax error; type: help")
            return True, None
        except Exception as e:
            print("ERR:", e)
            return True, None


    # def run(self):
    #     self.help()
    #     while True:
    #         try:
    #             line = input("> ")
    #         except (EOFError, KeyboardInterrupt):
    #             print(); break
    #         if not self.handle_line(line): break
