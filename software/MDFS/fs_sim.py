import struct


def entry(name, offset, size):
    s = struct.pack("i", size)
    s += struct.pack("i", offset)
    s += name
    s += b'\x00' * (128 - len(s))
    return s


text = """print("hello world!")"""




with open("test_fs", "wb") as f:
    empty = b'\x00' * 128
    file1 = bytes(text, 'ascii')# + b'\x00'
    block1 = file1 + b'\xff' * (65536 - len(file1))

    file_list = empty + entry(b'file1', 65536, len(file1)) + empty * (512-2)
    print(f.write(file_list + block1))
