# Force flash size to 4MB when upload uses "detect" (avoids esptool crash when
# flash chip is not detected, e.g. FlashID=0xffffff).
Import("env")
flags = env.get("UPLOADERFLAGS", [])
# UPLOADERFLAGS can be a list; replace "--flash_size", "detect" with "4MB"
new_flags = []
i = 0
while i < len(flags):
    f = flags[i]
    if f == "--flash_size" and i + 1 < len(flags) and flags[i + 1] == "detect":
        new_flags.append("--flash_size")
        new_flags.append("4MB")
        i += 2
        continue
    new_flags.append(f)
    i += 1
env.Replace(UPLOADERFLAGS=new_flags)
