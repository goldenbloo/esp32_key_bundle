def check_em4001_parity(tag_input_buff):
    """
    Checks the parity of a 64-bit EM4001 RFID tag value (MSB first).

    Args:
        tag_input_buff: A 64-bit integer representing the received tag data.

    Returns:
        A tuple containing:
            - is_parity_ok: True if parity check passes, False otherwise.
            - tag_id: The extracted 32-bit tag ID if parity is OK, None otherwise.
    """
    # Check if the first 9 bits are all 1 (preamble)
    if (tag_input_buff & 0xFF80000000000000) == 0xFF80000000000000:
        is_parity_ok = True

        # Parity by rows (5 data bits + 1 parity bit)
        for i in range(5, 55, 6):  # Iterate through the start of each 6-bit row
            parity = 0
            for j in range(5):
                parity ^= (tag_input_buff >> (i + j)) & 1
            # Check the parity bit (the 6th bit in the row)
            if parity != ((tag_input_buff >> (i + 5)) & 1):
                is_parity_ok = False
                break

        if is_parity_ok:
            # Parity by columns (1 data bit from each of the 5 rows + 1 overall parity bit)
            for i in range(1, 6):  # Iterate through the bit position within each row
                parity = 0
                for j in range(0, 55, 6):  # Iterate through the start of each row
                    parity ^= (tag_input_buff >> (i + j)) & 1
                # Check the overall parity bit (after the 5 row parity bits)
                if parity != ((tag_input_buff >> (i + 5 * 6)) & 1): # Adjusted index for column parity
                    is_parity_ok = False
                    break

        if is_parity_ok:
            tag_id = 0
            for i in range(6, 55, 6):  # Extract 4 data bits from each row
                tag_id = (tag_id << 4) | ((tag_input_buff >> i) & 0x0F)
            return is_parity_ok, tag_id
        else:
            return is_parity_ok, None
    else:
        return False, None

if __name__ == "__main__":
    # Example usage: Replace with your actual 64-bit tag input
    # Example 1: Valid tag data (you'll need to create a valid example)
    valid_tag_data = 0xFF8E2001A5761700  # Example - not guaranteed to be valid parity
    is_ok, tag_id = check_em4001_parity(valid_tag_data)
    print(f"Tag Data: 0x{valid_tag_data:016X}")
    print(f"Parity OK: {is_ok}")
    if is_ok and tag_id is not None:
        print(f"Tag ID: 0x{tag_id:08X}")

    print("-" * 20)

    # # Example 2: Invalid preamble
    # invalid_preamble_data = 0xFFE881F4E20C01F6
    # is_ok, tag_id = check_em4001_parity(invalid_preamble_data)
    # print(f"Tag Data: 0x{invalid_preamble_data:016X}")
    # print(f"Parity OK: {is_ok}")
    # if is_ok and tag_id is not None:
    #     print(f"Tag ID: 0x{tag_id:08X}")

    # print("-" * 20)

    # # Example 3: Tag data with potential parity error (you'll need to create one)
    # parity_error_data = 0xFF8844221100EE01 # Example - likely has parity error
    # is_ok, tag_id = check_em4001_parity(parity_error_data)
    # print(f"Tag Data: 0x{parity_error_data:016X}")
    # print(f"Parity OK: {is_ok}")
    # if is_ok and tag_id is not None:
    #     print(f"Tag ID: 0x{tag_id:08X}")