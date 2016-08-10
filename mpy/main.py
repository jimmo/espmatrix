import machine
import socket
import time

addr = socket.getaddrinfo('0.0.0.0', 80)[0][-1]

s = socket.socket()
s.bind(addr)
s.listen(1)

grid = [0]*8

tx_pin = machine.Pin(2, machine.Pin.OUT)
tx_pin.high()

spi = machine.SPI(baudrate=3300,polarity=1,phase=0,sck=machine.Pin(0),mosi=machine.Pin(2),miso=machine.Pin(4))

def send_grid():
  gc.disable()
  t = [0x55, 0xaa, 0, 0, 0, 0, 0, 0, 0, 0, 0]
  checksum = 0x55 ^ 0xaa
  for i in range(8):
    t[i+2] = grid[i]
    checksum ^= grid[i]
  t[10] = checksum

  b = bytearray(11*2)
  for i in range(11):
    b[i*2] = 64 + (t[i]>>2)
    b[i*2+1] = ((t[i]&3)<<6) | 31
  spi.write(b)
  gc.enable()

print('listening on', addr)

while True:
  cl, addr = s.accept()
  print('client connected from', addr)
  cl_file = cl.makefile('rwb', 0)
  path = '/'
  while True:
    line = cl_file.readline()
    if not line or line == b'\r\n':
      break
    if line.startswith('GET /'):
      path = line.decode().split(' ')[1]
  if path == '/':
    with open('squares.html') as f:
      for line in f:
        cl.send(line)
        time.sleep_ms(1)
  elif path.startswith('/set/'):
    parts = path.split('/')
    if len(parts) >= 5:
      x = int(parts[2])
      y = int(parts[3])
      v = int(parts[4])
      x = min(max(0, x), 7)
      y = min(max(0, y), 7)
      if v:
        grid[y] |= (1 << x)
      else:
        grid[y] &= ~(1 << x)
      for i in range(4):
        send_grid()
    cl.send('ok')
  else:
    cl.send('error')
  cl.close()
