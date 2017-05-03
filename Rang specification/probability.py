#!/usr/bin/python3

# Probability of a random xor matrix to have full rank

def full_rank_probability(width, height):
	p = 0

	for s in range(max(width, height), abs(width - height), -1):
		l = .5 ** s

		p *= 1 - l
		p += l

	return 1 - p

print("Probability to be non-singular:", full_rank_probability(1024, 1024))

success_probability = full_rank_probability(1024, 1024 + 24)

print("Success probability for 24 padding bits:", success_probability)

# Probability of a square matrix to need at least a given number of random additional rows to achieve full rank

def additional_height_probability(order, additional_height):
	return (
		full_rank_probability(order, order + additional_height) -
		(0 if additional_height == 0 else full_rank_probability(order, order + additional_height - 1))
	)

# Needed count of padding bits for multiple blocks

def minimum_padding_bits_count(block_size, blocks_count, success_probability):
	a = [additional_height_probability(block_size, i) for i in range(56)] # 56 iterations exhaust floating point precision
	m = [1]

	for i in range(blocks_count):
		n = [0] * (len(a) + len(m) - 1)

		for j in range(len(a)):
			for k in range(len(m)):
				n[j + k] += a[j] * m[k]

		m = n

	p = 0

	for i in range(len(m)):
		p += m[i]

		if p >= success_probability:
			return i

padding_bits_count = minimum_padding_bits_count(1024, 2, success_probability)

print("Padding bits for two blocks with at least the same success probability:", padding_bits_count)
