#! /usr/bin/env python

# https://wiki.osdev.org/CRC32

"""
//Fill the lookup table -- table = the lookup table base address
void crc32_fill(uint32_t *table){
        uint8_t index=0,z;
        do{
                table[index]=index;
                for(z=8;z;z--) 
                table[index]=(table[index]&1)?(table[index]>>1)^0xEDB88320:table[index]>>1;
        }while(++index);
}
"""


def generate_table(poly):
    table = [0 for i in range(256)]
    index = 0
    for i in range(256):
        table[i] = i
        for j in range(8):
            if table[i] & 0x1 > 0:
                table[i] = (table[i] >> 1) ^ poly
            else:
                table[i] = table[i] >> 1
    return table


def calc_crc(data, table):
    crc = 0xFFFFFFFF
    for x in data:
        crc = table[(crc ^ x) & 0xFF] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


def print_table(t):
    print("[")
    for i in range(256//4):
        print("0x{:08X}, 0x{:08X}, 0x{:08X}, 0x{:08X},".format(
                *t[i*4:i*4 + i + 4]))
    print("]")

# example table for CRC32 (ethernet etc.)
t = generate_table(0xEDB88320)
print_table(t)