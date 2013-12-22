import serial

EVENT_TIMER = 1
EVENT_TAP = 2

def put_seq_commands(port, commands):
    port.write('L' + chr(len(commands)))
    port.read(1) # should be '>'
    for event_type, event_arg, src_image, tgt_image in commands:
        port.write(chr(event_type))
        port.write(chr(event_arg))
        port.write(chr(src_image))
        port.write(chr(tgt_image))
        port.read(1) # should be '.'
    port.read(1) # should be '<'

if __name__ == '__main__':
    import sys
    device, = sys.argv[1:]
    commands = [
        [EVENT_TIMER, 0, i, (i+1) % 5] for i in range(5)
    ]
    port = serial.Serial(device)
    port.flushInput()
    put_seq_commands(port, commands)
