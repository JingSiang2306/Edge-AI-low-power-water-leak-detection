import argparse, os, shutil, subprocess, tempfile, glob

def run(cmd, cwd=None):
    print(" ".join(cmd))
    subprocess.check_call(cmd, cwd=cwd)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inlib", required=True)
    ap.add_argument("--out", dest="outlib", required=True)
    ap.add_argument("--prefix", required=True)  # e.g. neai_audio_
    ap.add_argument("--ar", default="arm-none-eabi-ar")
    ap.add_argument("--nm", default="arm-none-eabi-nm")
    ap.add_argument("--objcopy", default="arm-none-eabi-objcopy")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as td:
        # 1) extract objects
        run([args.ar, "x", os.path.abspath(args.inlib)], cwd=td)
        objs = [p for p in glob.glob(os.path.join(td, "*.o"))]
        if not objs:
            raise SystemExit("No .o files extracted. Is this a valid .a archive?")

        # 2) collect defined global symbols
        syms = set()
        for o in objs:
            out = subprocess.check_output([args.nm, "-g", "--defined-only", o], text=True)
            for line in out.splitlines():
                parts = line.split()
                if len(parts) >= 3:
                    sym = parts[2]
                    # skip empty or weird entries
                    if sym and sym != "_":
                        syms.add(sym)

        # 3) build mapping file
        mapfile = os.path.join(td, "symmap.txt")
        with open(mapfile, "w", newline="\n") as f:
            for s in sorted(syms):
                f.write(f"{s} {args.prefix}{s}\n")

        # 4) apply mapping to each object
        for o in objs:
            run([args.objcopy, f"--redefine-syms={mapfile}", o])

        # 5) repack
        out_abs = os.path.abspath(args.outlib)
        if os.path.exists(out_abs):
            os.remove(out_abs)
        run([args.ar, "rcs", out_abs] + [os.path.basename(o) for o in objs], cwd=td)

    print(f"Done: {args.outlib}")

if __name__ == "__main__":
    main()
