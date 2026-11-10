#include <iostream>
#include <cstdint>

/*struct block {
	uint32_t words[32]; //Defining 32 words of 32 bits
			    // TODO Need to look into verifying endianess check
	block* nextBlock = nullptr;
};*/

struct byte_string_t {
	uint64_t capacity = 1;
	uint64_t size = 0;
	uint8_t *bytes = new uint8_t[capacity];

	void grow() {
		uint8_t* tmp = new uint8_t[capacity * 2];
		
		for (uint64_t i = 0; i < size; i++) {
			tmp[i] = bytes[i];
		}
		capacity *= 2;

		delete[] bytes;
		bytes = tmp;
		tmp = nullptr;
	}

	void append(uint8_t byte) {
		bytes[size] = byte;
		size++;
		if (size == capacity) {
			grow();
		}
	}
};

byte_string_t read_message();
void pad_message(byte_string_t& messsage);

int main() {
	byte_string_t bytes = read_message();

	for (uint64_t i = 0; i < bytes.size; i++) {
		std::cout << bytes.bytes[i];
	}

	return 0;
}

byte_string_t read_message() {
	byte_string_t bytes;
	int ch;
	while ((ch = std::cin.get()) != EOF) {
		bytes.append(ch);
	}
	return bytes;
}

void pad_message(byte_string_t& messsage) {
	uint64_t BASE_BYTES = 448 / 8, BLOCK_BYTES = 64;
	uint64_t message_length = message.size * 8;

	message.append(0x80);
	while ( message.size % BLOCK_BYTES != BASE_BYTES ) {
		message.append(0x00);
	}
	
	uint32_t check = 1;
	bool is_big_endian = false;
	if ( check != 0x01000000 ) {
		is_big_endian = true;
	}
	if (!is_big_endian) {
		uint64_t tmp = 0;
		for (int i = 0; i < 8; i ++) {
			tmp += ((message_length >> 8 * (7 - i)) % 256) << 8 * (7 - i);
		}
		message_length = tmp;
	}
}
