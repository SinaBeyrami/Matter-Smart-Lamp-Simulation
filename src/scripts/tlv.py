def tlv_uint(tag: int, value: int, size=1) -> bytes:

    base = 0x04 | (tag << 5)
    if size == 1:
        return bytes([base | 0x00, 1, value & 0xFF])
    elif size == 2:
        return bytes([base | 0x01,
                      value & 0xFF,  (value >> 8) & 0xFF])
    raise ValueError("size must be 1 or 2")

def build_onoff(cmd: int) -> bytes:

    payload  = tlv_uint(0, 0)
    payload += tlv_uint(1, 0x0006, 2)
    payload += tlv_uint(2, cmd)
    return payload

def tlv_uint16(tag: int, value: int) -> bytes:
    return tlv_uint(tag, value, size=2)

def tlv_encode_bytes(tag: int, data: bytes) -> bytes:

    if len(data) > 255:
        raise ValueError("data too long (>255)")
    ctrl = 0x10 | (tag << 5)
    return bytes([ctrl, len(data)]) + data

def build_descriptor_request() -> bytes:
    p  = tlv_uint(0, 0)
    p += tlv_uint16(1, 0x001D)
    p += tlv_uint(2, 0x01)
    return p

def build_basic_request() -> bytes:
    p  = tlv_uint(0, 0)
    p += tlv_uint16(1, 0x0028)
    p += tlv_uint(2, 0x01)
    return p

def decode_basic_response(data: bytes) -> dict:
    i, result = 0, {}
    while i < len(data):
        ctrl = data[i]
        tag  = (ctrl >> 5) & 0x07
        base = ctrl & 0x1F
        i += 1

        if base & 0x1C == 0x04:
            len_code = base & 0x01
            if len_code == 0:
                length = data[i]; i += 1
                val = data[i]
                i += length
            else:
                val = data[i] | (data[i+1] << 8)
                i += 2

            if tag == 0: result["endpoint"]   = val
            if tag == 1: result["vendor_id"]  = val
            if tag == 2: result["product_id"] = val
            if tag == 3: result["sw_major"]   = val
        else:
            length = data[i]; i += 1 + length
    return result


def build_descriptor_request() -> bytes:
    p  = tlv_uint (0, 0)
    p += tlv_uint16(1, 0x001D)
    p += tlv_uint (2, 0x01)
    return p


def decode_descriptor_response(data: bytes) -> dict:
    result, i = {}, 0
    while i < len(data):
        ctrl = data[i]
        tag  = (ctrl >> 5) & 0x07
        base = ctrl & 0x1F
        i += 1

        if base & 0x1C == 0x04:
            len_code = base & 0x01
            if len_code == 0:
                length = data[i]
                i += 1
                val = data[i]
                i += length
            else:
                val = data[i] | (data[i + 1] << 8)
                i += 2

            if tag == 0:
                result["endpoint"] = val
            elif tag == 1:
                result["device_type"] = val
        else:
            length = data[i]
            i += 1
            if tag == 2:
                pairs = [
                    data[i + j] | (data[i + j + 1] << 8)
                    for j in range(0, length, 2)
                ]
                result["server_clusters"] = pairs
            i += length

    return result

def build_level_request(level: int) -> bytes:
    lvl = max(0, min(level, 254))
    p  = tlv_uint (0, 0)
    p += tlv_uint16(1, 0x0008)
    p += tlv_uint (2, 0x04)
    p += tlv_uint (3, lvl)
    return p

def build_timer_write(sec: int) -> bytes:
    p = tlv_uint (0,0)
    p += tlv_uint16(1, 0x0006)
    p += tlv_uint (2, 0x02)
    p += tlv_uint16(4, 0x4001)
    p += tlv_uint16(5, sec)
    return p

def build_timer_read() -> bytes:
    p = tlv_uint (0,0)
    p += tlv_uint16(1, 0x0006)
    p += tlv_uint (2, 0x03)
    p += tlv_uint16(4, 0x4001)
    return p

def decode_timer_resp(data: bytes) -> int | None:
    i = 0
    while i < len(data):
        ctrl = data[i]
        tag  = (ctrl >> 5) & 0x07
        base = ctrl & 0x1F
        i += 1

        if base & 0x1C == 0x04:
            len_code = base & 0x01
            if len_code == 0:
                length = data[i]
                i += 1 + length
            else:
                value = data[i] | (data[i + 1] << 8)
                i += 2
                if tag == 5:
                    return value
        else:
            length = data[i]
            i += 1 + length

    return None

def build_pbkdf_request() -> bytes:
    p  = tlv_uint(0, 0)
    p += tlv_uint16(1, 0x0000)
    p += tlv_uint(2, 0x01)
    p += tlv_uint16(3, 0x1234)
    p += tlv_encode_bytes(4, bytes(16))
    p += tlv_uint16(5, 8000)
    p += tlv_encode_bytes(6, bytes(40))
    return p

def build_level_read() -> bytes:
    p  = tlv_uint(0, 0)
    p += tlv_uint16(1, 0x0008)
    p += tlv_uint(2, 0x03)
    p += tlv_uint16(4, 0x0000)
    return p

def build_identify(sec:int)->bytes:
    p  = tlv_uint(0,0)
    p += tlv_uint16(1,0x0003)
    p += tlv_uint(2,0x00)
    p += tlv_uint16(0,sec)
    return p

def build_pake1() -> bytes:
    p  = tlv_uint(0, 0)
    p += tlv_uint16(1, 0x0000)
    p += tlv_uint(2, 0x03)
    p += tlv_encode_bytes(4, bytes(65))
    return p

def build_pake3() -> bytes:

    p  = tlv_uint(0, 0)
    p += tlv_uint16(1, 0x0000)
    p += tlv_uint(2, 0x05)
    return p