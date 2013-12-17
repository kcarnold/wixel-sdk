import re


def load_image(device, image, sector):
    with open(image) as f:
        image_data = f.read()
    s = ''.join(chr(int(b, 16)) for b in re.findall('0x([0-9a-fA-F]+)', image_data))
    with open(device, 'w+', 0) as f:
        f.write('u%02x' % sector)
        f.read(1)
        for i in range(0, len(s), 8):
            f.write(s[i:i + 8])
            f.read(1)
        f.read(1)

if __name__ == '__main__':
    import sys
    device, image, sector = sys.argv[1:]
    load_image(device, image, int(sector))
