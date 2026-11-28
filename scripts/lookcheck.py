#!/usr/bin/env python3
"""Check projected scene subjects against the corresponding screenshot pixels."""
import re
import sys
from PIL import Image


def main():
    if len(sys.argv) != 3:
        print('usage: lookcheck.py <frame.png> <pixel-log>', file=sys.stderr)
        return 1
    pattern = re.compile(r'\bPIXEL (\S+) (-?\d+) (-?\d+) (#[0-9A-Fa-f]{6})\b')
    with open(sys.argv[2], encoding='utf-8', errors='replace') as log:
        subjects = [m.groups() for line in log if (m := pattern.search(line))]
    if not subjects:
        print('FAIL palette no-subjects: 0')
        return 1
    failed = False
    with Image.open(sys.argv[1]) as frame:
        image = frame.convert('RGB')
        for name, sx, sy, colour in subjects:
            x, y = int(sx), int(sy)
            target = tuple(int(colour[i:i + 2], 16) for i in (1, 3, 5))
            count = 0
            if x >= 4 and y >= 4 and x + 4 < image.width and y + 4 < image.height:
                for py in range(y - 4, y + 5):
                    for px in range(x - 4, x + 5):
                        pixel = image.getpixel((px, py))
                        count += all(abs(pixel[i] - target[i]) <= 16 for i in range(3))
            print(f'palette {name} {colour}: {count}')
            if count < 20:
                print(f'FAIL palette {name} {colour}: {count}')
                failed = True
    if not failed:
        print('PASS palette')
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())
