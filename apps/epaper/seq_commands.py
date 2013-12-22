EVENT_TIMER = 1
EVENT_TAP = 2

def put_seq_commands(device, commands):
    with open(device, 'w+', 0) as f:
        f.write('L' + chr(len(commands)))
        f.read(1) # should be '>'
        for event_type, event_arg, src_image, tgt_image in commands:
            f.write(chr(event_type))
            f.write(chr(event_arg))
            f.write(chr(src_image))
            f.write(chr(tgt_image))
            f.read(1) # should be '.'
        f.read(1) # should be '<'

if __name__ == '__main__':
    import sys
    device, = sys.argv[1:]
    commands = [
        [EVENT_TIMER, 0, i, (i+1) % 5] for i in range(5)
    ]
    put_seq_commands(device, commands)
