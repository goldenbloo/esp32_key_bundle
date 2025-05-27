
import msvcrt
import serial
ser = serial.Serial('COM4', 115200)
ser.write(b'Hello, Serial!')
print("Press 'q' to exit.")
while True:
    if msvcrt.kbhit():
        char = msvcrt.getwch()
        ser.write(char.encode(encoding="ascii", errors="replace"))        
        if char == 'q':
            break
        print(f'You pressed: {char}')