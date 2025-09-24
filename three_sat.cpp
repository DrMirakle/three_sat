/* ========================================================================
   $File: three_sat.cpp $
   $Date: 2010/09/24 $
   $Revision: 6 $
   $Creator: Frederic Gillet $
   $Notice:

   The author of this software MAKES NO WARRANTY as to the RELIABILITY,
   SUITABILITY, or USABILITY of this software. USE IT AT YOUR OWN RISK.

   ======================================================================== */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef int8_t	int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32	bool32;

typedef uint8_t	 uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef char byte;

typedef float real32;
typedef double real64;

typedef size_t mem_index;

#define global_variable static
#define internal static
#define local_persist static

global_variable bool32 _running;


int assert(bool condition, char* error) {
	if (!condition) {
		printf("assert:");
		printf(error);
		exit(0);
	}
	return 1;
}

#define ASSERT(condition,error) if(!condition) {printf("assert: "); printf(error); exit(0);}

struct Buffer {
	mem_index length = 0;
	mem_index SIZE = 0;
	char* address = 0;
};

void buffer_init(Buffer* buffer, mem_index size) {
	buffer->SIZE = size;
	buffer->address = (char*)malloc(size);
	buffer->length = 0;
}

void buffer_free(Buffer* buffer) {
	if (buffer->address != 0) {
		free(buffer->address);
	}
}

void buffer_reset(Buffer* buffer) {
	buffer->length = 0;
}

void buffer_clone(Buffer* dst, Buffer* src) {
	dst->length = src->length;
	memcpy(dst->address, src->address, src->length);
}


void buffer_shift_right(Buffer* buf, mem_index index, mem_index shift) {

	assert(buf->length + shift <= buf->SIZE, "buffer overflow");
	assert(index <= buf->length, "bleh"); //equality so we can append at the end
	assert(shift > 0, "bleh");

	mem_index n = buf->length - index; //n == 0 if nothing to shift, we just extend the length

	if (n > 0) {
		memcpy(buf->address + index + shift, buf->address + index, n); 
	}
	buf->length += shift;
}

void buffer_shift_left(Buffer* buf, mem_index index, mem_index shift) {

	assert(buf->length - shift >= 0, "overflow");
	assert(index <= buf->length, "overflow");
	assert(shift > 0, "overflow");

	mem_index n = buf->length - index;

	//if n == 0, there's nothing to move, we just crop
	if (n > 0) {
		memcpy(buf->address + index - shift, buf->address + index, n);
	}
	buf->length -= shift;
}

void buffer_write_byte(Buffer* buf, char val, mem_index index) {
	assert(index <= buf->length, "write_char overflow");
	assert(index >= 0, "write_char negative index");
	if (index == buf->length) {
		assert(buf->length + 1 <= buf->SIZE, "write_char overflow");
		buf->length += 1;
	}
	*(buf->address + index) = val;
}

void buffer_write_integer(Buffer* buf, mem_index index, int value, int bytes) {
	memcpy(buf->address + index, &value, bytes);
}

void buffer_write_(Buffer* buf, mem_index index, void* value, size_t bytes) {
	mem_index overwrite = index + bytes - buf->length;
	if (overwrite > 0) {
		assert(buf->length + overwrite <= buf->SIZE, "write overflow");
	}
	memcpy(buf->address + index, value, bytes);
}

void memory_write(char* address, mem_index index, mem_index offset, void* value, size_t bytes) {
	memcpy(address + index + offset, value, bytes);
}

uint32 memory_read_integer(char* address, mem_index index, int bytes) {
	uint32 value = 0;
	for (int b = 0; b < bytes; b++) {
		int v = ((char) *(address + index + b) & 0xFF) << b * 8;
		value = value | v;
	}
	return value;
}

void buffer_append_(Buffer* buf, void* value, size_t bytes) {
	buffer_write_(buf, buf->length, value, bytes);
}

void* buffer_read_(Buffer* buf, mem_index offset) {
	return (buf->address + offset);
}

#define buffer_append(BUFFER,VALUE,TYPE) buffer_append_(&BUFFER,&VALUE,sizeof(TYPE))
#define buffer_write(BUFFER,OFFSET,VALUE,TYPE) buffer_write_(&BUFFER,OFFSET,&VALUE,sizeof(TYPE))
#define buffer_read(BUFFER,OFFSET,TYPE) *(TYPE*)buffer_read_(&BUFFER,OFFSET)

void buffer_append_byte(Buffer* buf, char val) {
	buffer_write_byte(buf, val, buf->length);
}

void buffer_append_string(Buffer* buf, char* string) {
	int i = 0;
	while (string[i] != '\0') {
		buffer_append_byte(buf, string[i]);
		i++;
	}
}

void buffer_write_bytes(Buffer* dst, mem_index dst_index, Buffer* src, mem_index src_index, mem_index n) {
	assert(dst_index <= dst->length, "out of range index");
	assert(dst_index >= 0, "out of range index");
	if (dst_index + n >= dst->length) {
		assert(dst_index + n <= dst->SIZE, "write_bytes overflow");
		dst->length = dst_index + n;
	}
	memcpy(dst->address + dst_index, src->address + src_index, n);
}

void buffer_append_bytes(Buffer* dst, Buffer* src, mem_index src_index, mem_index n) {
	buffer_write_bytes(dst, dst->length, src, src_index, n);
}

char buffer_read_byte(Buffer* buf, mem_index index) {
	assert(index >= 0, "read_byte negative index");
	assert(index < buf->SIZE, "read_byte index overflow");
	assert(index < buf->length, "read_byte index overflow");
	char c = (char) *(buf->address + index);
	return c;
}

void buffer_read_bytes(Buffer* dst, Buffer* src, mem_index src_index, mem_index n) {
	assert(src_index >= 0, "read_bytes negative index");
	assert(src_index < src->SIZE, "read_bytes index overflow");
	assert(src_index < src->length, "read_bytes index overflow");
	assert(src_index + n <= src->length, "read_bytes index overflow");
	dst->length = n;
	memcpy(dst->address, src->address + src_index, n);
}

int buffer_read_integer(Buffer* buf, mem_index index, int bytes) {
	int value = 0;
	for (int b = 0; b < bytes; b++) {
		int v = ((char) *(buf->address + index + b) & 0xFF) << b * 8;
		value = value | v;
	}
	return value;
}

bool32 buffer_compare(Buffer* dst, mem_index dst_index, Buffer* src, mem_index src_index, int n) {
	assert(dst_index + n <= dst->length, "out of range");
	assert(src_index + n <= src->length, "out of range");
	for (int i = 0; i < n; i++) {
		char src_char = buffer_read_byte(src, src_index + i);
		char dst_char = buffer_read_byte(dst, dst_index + i);
		if (src_char != dst_char) {
			return false;
		}
	}
	return true;
}

void buffer_to_string(Buffer* buf, char* s, size_t s_length) {
	mem_index char_count = buf->length > (s_length - 1) ? (s_length - 1) : buf->length;
	for (uint32 i = 0; i < char_count; i++) {
		s[i] = buffer_read_byte(buf, i);
	}
	s[char_count] = 0;
}

struct Allocator {
	uint32 SEGMENT_HEADER_SIZE; //size in bytes of the header in the data that's recording the index of the segment
	uint32 SEGMENT_COUNT;
	mem_index TOTAL_SIZE;
	mem_index free_size;
	char* address;
	mem_index* segment_mem_index;
	mem_index* segment_size;
	uint32* segment_ref_index; //indirection so we can realloc
	uint32 free_segment_index;
	uint32 max_free_segment_index;
};

void allocator_init(Allocator* alloc, mem_index size, mem_index bytes_for_segment_count) {
	alloc->SEGMENT_COUNT = 1 << (8 * bytes_for_segment_count);
	alloc->SEGMENT_HEADER_SIZE = bytes_for_segment_count;
	alloc->TOTAL_SIZE = size;
	alloc->free_size = size;
	alloc->free_segment_index = 0;
	alloc->max_free_segment_index = 0;
	alloc->address = (char*)malloc(alloc->TOTAL_SIZE);
	alloc->segment_mem_index = (mem_index*)malloc(alloc->SEGMENT_COUNT * sizeof(mem_index));
	alloc->segment_ref_index = (uint32*)malloc(alloc->SEGMENT_COUNT * sizeof(uint32));
	alloc->segment_size = (mem_index*)malloc(alloc->SEGMENT_COUNT * sizeof(mem_index));
	for (uint32 i = 0; i < alloc->SEGMENT_COUNT; i++) {
		alloc->segment_mem_index[i] = -1;
		alloc->segment_ref_index[i] = -1;
		alloc->segment_size[i] = -1;
	}
	alloc->segment_mem_index[0] = 0;
	alloc->segment_ref_index[0] = 0;
	alloc->segment_size[0] = alloc->TOTAL_SIZE;
}

void allocator_print(Allocator* alloc, int element_count) {
	printf("alloc\n====\n");
	printf("free_segment_index: %d\n", alloc->free_segment_index);
	printf("free_size: %zd\n", alloc->free_size);
	for(int i = 0; i < element_count; i++) {
		printf("mem_index[%d]: %zd - size[%d]: %zd\n", i, alloc->segment_mem_index[i], i, alloc->segment_size[i]);
	}
}

void allocator_free(Allocator* alloc) {
	if (alloc->address != 0) {
		free(alloc->address);
	}
}

int allocator_segment_create(Allocator* alloc, mem_index size) {

	//print_allocator(alloc, 10);

	mem_index total_size_to_allocate = size + alloc->SEGMENT_HEADER_SIZE;
	if (alloc->free_segment_index == alloc->SEGMENT_COUNT) {
		assert(false, "out of free segments");
		return -1;
	}

	if (alloc->free_size < total_size_to_allocate) {
		allocator_print(alloc, 20);
		assert(false, "allocator_segment_create - new segment size is too large");
		return -1;
	}

	assert(alloc->segment_size[alloc->free_segment_index] != -1, "free segment should not be empty");
	assert(alloc->segment_mem_index[alloc->free_segment_index] != -1, "free segment should not be empty");

	bool found = false;
	uint32 index = alloc->free_segment_index;
	while (index < alloc->SEGMENT_COUNT) {
		mem_index free_size = alloc->segment_size[index];
		if (free_size != -1 && free_size >= total_size_to_allocate) { 
			found = true;
			break;
		}
		index++;
	}
	if (!found) {
		assert(false, "out of memory");
		return -1;
	}
	if (index > alloc->free_segment_index) {
		//swap index data with free index data
		mem_index free_size = alloc->segment_size[index];
		mem_index free_index = alloc->segment_mem_index[index];

		alloc->segment_size[index] = alloc->segment_size[alloc->free_segment_index];
		alloc->segment_mem_index[index] = alloc->segment_mem_index[alloc->free_segment_index];
		alloc->segment_size[alloc->free_segment_index] = free_size;
		alloc->segment_mem_index[alloc->free_segment_index] = free_index;
	}
	//fetch a new index
	mem_index segment_mem_index = alloc->segment_mem_index[alloc->free_segment_index];
	mem_index data_mem_index = segment_mem_index + alloc->SEGMENT_HEADER_SIZE;
	memory_write(alloc->address, segment_mem_index, 0, &alloc->free_segment_index, alloc->SEGMENT_HEADER_SIZE);
	mem_index free_size = alloc->segment_size[alloc->free_segment_index];
	alloc->segment_size[alloc->free_segment_index] = total_size_to_allocate;
	alloc->free_size -= total_size_to_allocate;
	alloc->free_segment_index++;
	if (alloc->free_segment_index > alloc->max_free_segment_index) {
		alloc->max_free_segment_index = alloc->free_segment_index;
	}
	if (alloc->free_segment_index >= alloc->SEGMENT_COUNT) {
		return data_mem_index; 
	}
	mem_index extra_free_size = free_size - total_size_to_allocate;
	mem_index extra_mem_index = segment_mem_index + total_size_to_allocate;
	if (extra_free_size == 0) {
		return data_mem_index;
	}
	bool found_empty_slot = false;
	index = alloc->free_segment_index;
	while (index < alloc->SEGMENT_COUNT) {
		mem_index size = alloc->segment_size[index];
		if (size == -1) {
			alloc->segment_size[index] = extra_free_size;
			alloc->segment_mem_index[index] = extra_mem_index;
			return data_mem_index;
		}
		index++;
	}
	//look at any non empty free slots
	uint32 next_index = alloc->free_segment_index + 1;
	while (next_index < alloc->SEGMENT_COUNT) {
		mem_index size = alloc->segment_size[next_index];
		mem_index free_mem_index = alloc->segment_mem_index[next_index];
		if (size != -1 && alloc->segment_mem_index[alloc->free_segment_index] + alloc->segment_size[alloc->free_segment_index + extra_free_size == free_mem_index]) {
			alloc->segment_size[next_index] = alloc->segment_size[next_index] + extra_free_size;
			alloc->segment_mem_index[next_index] = alloc->segment_mem_index[next_index] - extra_free_size;
			return data_mem_index;
		}
	}
	return data_mem_index;
}

void allocator_segment_free(Allocator* alloc, mem_index data_index) { 
	//print_allocator(alloc, 10);
	mem_index header_index = data_index - alloc->SEGMENT_HEADER_SIZE;
	assert(header_index >= 0, "header index should not be negative");
	uint32 index = memory_read_integer(alloc->address, header_index, alloc->SEGMENT_HEADER_SIZE);
	assert(index < alloc->free_segment_index, "index out of bounds");
	assert(alloc->free_segment_index > 0 && alloc->free_segment_index <= alloc->SEGMENT_COUNT, "invalid free segment index");
	alloc->free_size += alloc->segment_size[index];
	alloc->free_segment_index--; //free_segment_index now points to an allocated segment

	if (alloc->free_segment_index > index) {
		mem_index free_size = alloc->segment_size[index];
		mem_index free_mem_index = alloc->segment_mem_index[index];
		alloc->segment_size[index] = alloc->segment_size[alloc->free_segment_index];
		alloc->segment_mem_index[index] = alloc->segment_mem_index[alloc->free_segment_index];
		//the segment pointed to by free_segment_index is allocated, so we need to adjust its header index to index
		memory_write(alloc->address, alloc->segment_mem_index[alloc->free_segment_index], 0, &index, alloc->SEGMENT_HEADER_SIZE);
		
		alloc->segment_size[alloc->free_segment_index] = free_size;
		alloc->segment_mem_index[alloc->free_segment_index] = free_mem_index;
	}
	uint32 next_index = alloc->free_segment_index + 1;
	while (next_index < alloc->SEGMENT_COUNT) {
		mem_index next_free_size = alloc->segment_size[next_index];
		mem_index next_free_mem_index = alloc->segment_mem_index[next_index];
		if (next_free_size == -1) {
			bool found = false;
			uint32 forward_index = next_index + 1;
			while (forward_index < alloc->SEGMENT_COUNT) {
				mem_index forward_size = alloc->segment_size[forward_index];
				int forward_mem_index = alloc->segment_mem_index[forward_index];
				if (forward_size != -1) {
					alloc->segment_size[forward_index] = -1;
					alloc->segment_mem_index[forward_index] = -1;
					alloc->segment_size[next_index] = forward_size;
					alloc->segment_mem_index[next_index] = forward_mem_index;
					found = true;
					break;
				}
				forward_index++;
			}
			if (!found) {
				break;
			}
		}
		next_free_size = alloc->segment_size[next_index];
		next_free_mem_index = alloc->segment_mem_index[next_index];
		mem_index next_mem_index = alloc->segment_mem_index[alloc->free_segment_index] + alloc->segment_size[alloc->free_segment_index];
		assert(next_free_size != -1, "slot should not be empty");
		assert(next_free_mem_index != -1, "slot should not be empty");
		if (next_mem_index == next_free_mem_index) {
			alloc->segment_size[next_index] = -1;
			alloc->segment_mem_index[next_index] = -1;
			alloc->segment_size[alloc->free_segment_index] = alloc->segment_size[alloc->free_segment_index] + next_free_size;
		}
		else if (alloc->segment_mem_index[alloc->free_segment_index] == next_free_mem_index + next_free_size) {
			//segments are contiguous but in reverse order
			alloc->segment_size[alloc->free_segment_index] = alloc->segment_size[alloc->free_segment_index] + next_free_size;
			alloc->segment_mem_index[alloc->free_segment_index] = next_free_mem_index;
			alloc->segment_size[next_index] = -1;
			alloc->segment_mem_index[next_index] = -1;
			next_index--; //rewind by one to check if we can swap the new empty slot
		}
		next_index++;
	}
	//print_allocator(alloc, 10);
}

struct List {
	size_t element_size;
	Allocator* alloc;
	uint32 size; //max size in elements, will double in size when extending
	uint32 length; //number of elements allocated
	mem_index index; //indirect index from allocator
	bool fixed;
};

void list_init(List* list, Allocator* allocator, size_t element_size, uint32 size, bool fixed_array) {
	list->alloc = allocator;
	list->element_size = element_size;
	list->size = size;
	list->length = 0;
	list->fixed = fixed_array;
	if (fixed_array) {
		list->length = size;
	}
	list->index = allocator_segment_create(list->alloc, list->size * list->element_size);
}

void list_resize(List* list) {
	list->size *= 2;
	mem_index old_mem_index = list->index;
	mem_index new_mem_index = allocator_segment_create(list->alloc, list->size * list->element_size);
	//copy the old data to the new location
	memcpy(list->alloc->address + new_mem_index, list->alloc->address + old_mem_index, list->length * list->element_size);
	allocator_segment_free(list->alloc, old_mem_index);
	list->index = new_mem_index;
}

void list_add(List* list, void* element) {
	int new_element_index = list->length;
	assert(!list->fixed, "cannot add to a fixed list");
	if (list->length == list->size) {
		//list needs to grow
		list_resize(list);
	}
	list->length++;
	memory_write(list->alloc->address, list->index, new_element_index * list->element_size, element, list->element_size);
}

void* list_read_(List* list, uint32 element_index, size_t element_size) {
	assert(list->element_size == element_size, "element size mismatch");
	return(list->alloc->address + list->index + element_index * element_size);
}

#define list_read(LIST,INDEX,TYPE) *(TYPE*)list_read_(LIST,INDEX,sizeof(TYPE))

void list_remove(List* list, uint32 element_index) {
	assert(element_index < list->length, "list remove out of bounds");
	assert(element_index >= 0, "list remove index out of bounds");
	assert(!list->fixed, "cannot remove element from fixed list");
	int number_elements_after_removed_element = list->length - element_index - 1;
	if (number_elements_after_removed_element > 0) {
		memcpy(list->alloc->address + list->index + element_index * list->element_size, list->alloc->address + list->index + (element_index + 1)*list->element_size, number_elements_after_removed_element * list->element_size);
	}
	list->length--;
}

void list_clear(List* list) {
	assert(!list->fixed, "cannot clear fixed list");
	list->length = 0;
}

void list_set_to_zero(List* list) {
	memset(list->alloc->address + list->index, 0, list->length * list->element_size);
}

void list_free(List* list) {
	allocator_segment_free(list->alloc, list->index);
	list->fixed = false;
	list_clear(list);
}

void list_set(List* list, uint32 element_index, void* element) {
	assert(element_index >= 0, "list remove index out of bounds");
	assert(element_index < list->length, "index beyond current length");
	memory_write(list->alloc->address, list->index, element_index * list->element_size, element, list->element_size);
}


void list_insert(List* list, uint32 element_index, void* element) {
	assert(!list->fixed, "cannot insert in fixed list");
	assert(element_index <= list->length, "insert index out of range");
	if (list->length == list->size) {
		//list needs to grow
		list_resize(list);
	}

	if (element_index < list->length) {
		char* src_address = list->alloc->address + list->index + (element_index * list->element_size);
		char* dst_address = src_address + list->element_size;
		memcpy(dst_address, src_address, list->element_size*(list->length - element_index));
	}
	list->length++;
	list_set(list, element_index, element);
}

void list_print_real32(List* list) {
	printf("\n");
	for (uint32 i = 0; i < list->length; i++) {
		printf("%.10lf\n",list_read(list, i, real32));
	}
}

void list_print_real64(List* list) {
	printf("\n");
	for (uint32 i = 0; i < list->length; i++) {
		printf("%.10lf\n", list_read(list, i, real64));
	}
}

void list_print_uint32(List* list) {
	printf("list:\n");
	for (uint32 i = 0; i < list->length; i++) {
		printf("%d : %d\n", i, list_read(list, i, uint32));
	}
}

void list_print_uint64(List* list) {
	printf("list:\n");
	for (uint32 i = 0; i < list->length; i++) {
		printf("%d : %llu\n", i, list_read(list, i, uint64));
	}
}

void list_print_mem_index(List* list) {
	printf("list:\n");
	for (uint32 i = 0; i < list->length; i++) {
		printf("%d : %zu\n", i, list_read(list, i, mem_index));
	}
}

void list_print_int32(List* list) {
	printf("list:\n");
	for (uint32 i = 0; i < list->length; i++) {
		printf("%d : %d\n", i, list_read(list, i, int32));
	}
}

void list_print_int64(List* list) {
	printf("list:\n");
	for (uint32 i = 0; i < list->length; i++) {
		printf("%d : %lld\n", i, list_read(list, i, int64));
	}
}

#define list_print(LIST,TYPE) for(int i = 0; i < LIST->length; i++) {printf("%d",list_read(LIST,i,TYPE));}

void d_list_init(List* dlist, Allocator* alloc, uint32 size) {
	dlist->index = allocator_segment_create(alloc, size * sizeof(List));
	dlist->size = size;
	dlist->length = 0;
	dlist->element_size = sizeof(List);
	dlist->alloc = alloc;
	dlist->fixed = false;
}

List* d_list_create_element_list(List* dlist, size_t element_size, uint32 size, bool fixed) {
	List element_list;
	list_init(&element_list, dlist->alloc, element_size, size, fixed);
	list_add(dlist, &element_list);
	return (List*)list_read_(dlist, dlist->length - 1, dlist->element_size);
}

void d_list_remove_element_list(List* dlist, uint32 element_index) {
	assert(element_index >= 0, "d_list_read negative element index");
	assert(element_index < dlist->length, "d_list_read out of bounds element index");
	List* element_list = (List*)list_read_(dlist, element_index, dlist->element_size);
	list_free(element_list);
	list_remove(dlist, element_index);
}

List* d_list_read(List* dlist, uint32 element_index) {
	assert(element_index >= 0, "d_list_read negative element index");
	assert(element_index < dlist->length, "d_list_read out of bounds element index");
	return (List*)list_read_(dlist, element_index, dlist->element_size);
}

void d_list_free(List* dlist) {
	for (uint32 i = 0; i < dlist->length; i++) {
		List* element_list = (List*)list_read_(dlist, i, dlist->element_size);
		allocator_segment_free(element_list->alloc, element_list->index);
	}
	allocator_segment_free(dlist->alloc, dlist->index);
	list_clear(dlist);
}

void test_buffer() {

	Allocator all;
	allocator_init(&all, 200, 1);
	List list;
	list_init(&list, &all, 4, sizeof(real32), false);
	real32 r0 = 1.45f;
	real32 r1 = 1.451f;
	real32 r2 = 1.452f;
	real32 r3 = 1.453f;
	real32 r4 = 1.454f;
	list_add(&list, &r0);
	list_add(&list, &r1);
	list_add(&list, &r2);
	list_add(&list, &r3);
	list_add(&list, &r4);
	assert(list.length == 5, "");
	real32 rr = list_read(&list, 0, real32);
	assert(rr == r0, "");

	allocator_init(&all, 200, 1);
	
	list_init(&list, &all, 4, sizeof(real32), false);
	uint32 v0 = 45000;
	uint32 v1 = 46001;
	uint32 v2 = 47002;
	uint32 v3 = 48003;
	uint32 v4 = 49004;
	list_add(&list, &v0);
	list_add(&list, &v1);
	list_add(&list, &v2);
	list_add(&list, &v3);
	list_add(&list, &v4);
	assert(list.length == 5, "");
	uint32 vv = list_read(&list, 0, uint32);
	assert(vv == v0, "");
	vv = list_read(&list, 1, uint32);
	assert(vv == v1, "");
	vv = list_read(&list, 2, uint32);
	assert(vv == v2, "");
	vv = list_read(&list, 3, uint32);
	assert(vv == v3, "");
	vv = list_read(&list, 4, uint32);
	assert(vv == v4, "");

	list_remove(&list, 0);
	assert(list.length == 4, "");
	vv = list_read(&list, 0, uint32);
	assert(vv == v1, "");
	vv = list_read(&list, 1, uint32);
	assert(vv == v2, "");
	vv = list_read(&list, 2, uint32);
	assert(vv == v3, "");
	vv = list_read(&list, 3, uint32);
	assert(vv == v4, "");

	list_remove(&list, 3);
	assert(list.length == 3, "");
	list_remove(&list, 1);
	assert(list.length == 2, "");
	vv = list_read(&list, 0, uint32);
	assert(vv == v1, "");
	vv = list_read(&list, 1, uint32);
	assert(vv == v3, "");
	list_remove(&list, 0);
	assert(list.length == 1, "");
	vv = list_read(&list, 0, uint32);
	assert(vv == v3, "");
	list_remove(&list, 0);
	assert(list.length == 0, "");

	Buffer b1;
	Buffer b2;
	buffer_init(&b1, 5);
	buffer_init(&b2, 5);
	buffer_write_byte(&b1, 'a', 0);
	assert(b1.length == 1, "");
	char c = buffer_read_byte(&b1, 0);
	assert(c == 'a', "");
	assert(b1.length == 1, "");
	buffer_write_byte(&b1, 'b', 0);
	c = buffer_read_byte(&b1, 0);
	assert(c == 'b', "");
	assert(b1.length == 1, "");
	buffer_append_byte(&b1, 'c');
	buffer_read_bytes(&b2, &b1, 0, 2);
	c = buffer_read_byte(&b2, 0);
	assert(c == 'b', "");
	c = buffer_read_byte(&b2, 1);
	assert(c == 'c', "");
	char str[10];
	buffer_to_string(&b2, str, 10);
	assert(strcmp(str, "bc") == 0, "");

	buffer_shift_right(&b2, 1, 1);
	buffer_write_byte(&b2, 'a', 1);
	buffer_to_string(&b2, str, 10);
	assert(strcmp(str, "bac") == 0, "");

	buffer_shift_left(&b2, 2, 1);
	buffer_to_string(&b2, str, 10);
	assert(strcmp(str, "bc") == 0, "");

	buffer_reset(&b2);
	buffer_append_byte(&b2, 'a');
	buffer_append_byte(&b2, 'b');
	buffer_append_byte(&b2, 'c');
	buffer_append_byte(&b2, 'd');
	buffer_append_byte(&b2, 'e');
	buffer_to_string(&b2, str, 10);
	assert(strcmp(str, "abcde") == 0, "");

	buffer_reset(&b2);
	buffer_append_byte(&b2, 'a');
	buffer_append_byte(&b2, 'b');
	buffer_append_byte(&b2, 'c');
	buffer_reset(&b1);
	buffer_append_byte(&b1, '1');
	buffer_append_byte(&b1, '2');
	buffer_append_byte(&b1, '3');
	buffer_append_byte(&b1, '4');
	buffer_write_bytes(&b2, 1, &b1, 0, b1.length);
	buffer_to_string(&b2, str, 10);
	assert(strcmp(str, "a1234") == 0, "");

	buffer_reset(&b2);
	buffer_append_byte(&b2, 'a');
	buffer_append_byte(&b2, 'b');
	buffer_append_byte(&b2, 'c');
	//0 1 2 3 4
	//a b c
	buffer_shift_right(&b2, 1, 2);
	//0 1 2 3 4
	//a     b c
	buffer_reset(&b1);
	buffer_append_byte(&b1, '1');
	buffer_append_byte(&b1, '2');
	buffer_write_bytes(&b2, 1, &b1, 0, 2);
	buffer_to_string(&b2, str, 10);
	assert(strcmp(str, "a12bc") == 0, "");

}

struct Clause {
	uint32 i0;
	uint32 i1;
	uint32 i2;
	//direct or reverse
	bool b0;
	bool b1;
	bool b2;
};

void clause_copy(Clause* dst, Clause* src) {
	dst->i0 = src->i0;
	dst->i1 = src->i1;
	dst->i2 = src->i2;
	dst->b0 = src->b0;
	dst->b1 = src->b1;
	dst->b2 = src->b2;
}

bool clause_equal(Clause* c1, Clause* c2) {
	return	(c1->b0 == c2->b0) && 
			(c1->b1 == c2->b1) && 
			(c1->b2 == c2->b2) && 
			(c1->i0 == c2->i0) && 
			(c1->i1 == c2->i1) && 
			(c1->i2 == c2->i2);
}

bool clause_contains_var(Clause* clause, int i) {
	return (clause->i0 == i || clause->i1 == i || clause->i2 == i);
}

uint32 clause_find_index(Clause* clause, uint32 i) {
	if (clause->i0 == i) {
		return 0;
	}
	else if (clause->i1 == i) {
		return 1;
	}
	else if (clause->i2 == i) {
		return 2;
	}
	return -1;
}

bool clause_find_var_sign(Clause* clause, int i) {
	int index = clause_find_index(clause, i);
	if (index == 0) {
		return clause->b0;
	}
	else if (index == 1) {
		return clause->b1;
	}
	else if (index == 2) {
		return clause->b2;
	}
	return false;
}

bool clause_evaluate(Clause* clause, bool v0, bool v1, bool v2) {
	return ((clause->b0 ? v0 : !v0) || (clause->b1 ? v1 : !v1) || (clause->b2 ? v2 : !v2));
}

bool clause_is_satisfied(Clause* clause, bool v0, bool v1, bool v2) {
	return clause_evaluate(clause, v0, v1, v2);
}

void clause_test() {
	Clause c;
	c.i0 = 0;
	c.i1 = 1;
	c.i2 = 2;
	c.b0 = true;
	c.b1 = false; 
	c.b2 = true;
	assert(clause_is_satisfied(&c, true, true, false), "");
	assert(!clause_is_satisfied(&c, false, true, false), "");
}

void list_print_clause(List* list) {
	printf("list:\n");
	for (uint32 i = 0; i < list->length; i++) {
		Clause c = list_read(list, i, Clause);
		printf("(%d,%d,%d)\n", c.b0 ? c.i0 : -1 * c.i0, c.b1 ? c.i1 : -1 * c.i1, c.b2 ? c.i2 : -1 * c.i2);
	}
}

void list_print_bool(List* list) {
	printf("list:\n");
	for (uint32 i = 0; i < list->length; i++) {
		bool v = list_read(list, i, bool);
		printf("var %d = %d/n", i, v);
	}
}

void debug_list(List* list, mem_index* ccc, Allocator* alloc) {

	for (uint32 j = 0; j < list->length; j++) {
		mem_index tt = list_read(list, j, mem_index);
		mem_index c = ccc[j];

		if (c != tt) {
			//address in array
			mem_index* tt_address = (mem_index*)(list->alloc->address + list->index + j * list->element_size);
			mem_index ttt = *tt_address;
			int max = j + 10 > list->length ? list->length : j + 10;
			for (int i = j - 10; i < max; i++) {
				printf("ccc[%d] : %zd -- list[%d] : %zd\n", i, ccc[i], i, list_read(list, i, mem_index));

			}
			allocator_print(alloc, 20);
		}
	}
}

void debug_print_list(List* list, mem_index* ccc) {

	for (uint32 i = 0; i < list->length; i++) {
		printf("ccc[%d] : %zd -- list[%d] : %zd\n", i, ccc[i], i, list_read(list, i, mem_index));
	}
}

//constants for clause tree.
uint32 S1 = 1;
uint32 S2 = 2;
char C_s = '|';
char C_0 = '0';
char C_1 = '1';
char C_x = 'x';

mem_index segment_length(Buffer* tree, mem_index start_index) {
	//look for length until next separator or end of tree
	mem_index end_index = tree->length - 1;
	mem_index index = start_index;
	mem_index length = 0;
	while (index <= end_index) {
		if (buffer_read_byte(tree, index) == C_s) {
			return length;
		}
		else {
			index++;
			length++;
		}
	}
	return length;
}

mem_index start_index_of_next_branch(Buffer* tree, mem_index start_index) {
	mem_index end_index = tree->length - 1;
	mem_index index = start_index;
	while (index <= end_index) {
		if (C_s == buffer_read_byte(tree, index)) {
			return index + 1;
		}
		else {
			index++;
		}
	}
	return -1;
}

mem_index length_of_subtree(Buffer* tree, mem_index start_index) {

	mem_index current_length = segment_length(tree, start_index);
	mem_index total_length = current_length;
	if (current_length > 0) {
		mem_index next_branch_index = -1;
		while (true) {
			next_branch_index = start_index_of_next_branch(tree, start_index);
			if (next_branch_index != -1) {
				mem_index next_segment_length = segment_length(tree, next_branch_index);
				if (next_segment_length > current_length) {
					break;
				}
				else {
					//this segment is part of the subtree
					start_index = next_branch_index;
					total_length += next_segment_length + 1; //add 1 to account for '|'
				}
			}
			else {
				break; //we reach the end and there's no sibling branch
			}
		}
	}
	return total_length;
}

mem_index sibling_branch_index(Buffer* tree, mem_index t_index) {
	mem_index t_subindex = t_index;
	mem_index current_length = segment_length(tree, t_subindex);
	if (current_length > 0) {
		mem_index next_branch_index = -1;
		while (true) {
			next_branch_index = start_index_of_next_branch(tree, t_subindex);
			if (next_branch_index != -1) {
				mem_index next_segment_length = segment_length(tree, next_branch_index);
				if (next_segment_length == current_length) {
					//we found the sibling segment
					t_subindex = next_branch_index;
					break;
				}
				else if (next_segment_length > current_length) {
					t_subindex = -1;
					break;
				}
				else {
					//keep going.
					t_subindex = next_branch_index;
				}
			}
			else {
				//no next branch
				t_subindex = -1;
				break;
			}
		}
	}
	return t_subindex;
}

mem_index start_index_of_sibling_subtree(Buffer* tree, mem_index s_index) {

	mem_index t_index = s_index;
	mem_index r_index = -1;
	mem_index current_length = segment_length(tree, s_index);
	if (current_length > 0) {
		mem_index next_branch_index = -1;
		while (true) {
			next_branch_index = start_index_of_next_branch(tree, t_index);
			if (next_branch_index != -1) {
				mem_index next_segment_length = segment_length(tree, next_branch_index);
				if (next_segment_length == current_length) {
					//we found the sibling segment, we break to keep marching on this one
					r_index = next_branch_index;
					break;
				}
				else if (next_segment_length > current_length) {
					assert(false, "no sibling branch");
				}
				else {
					//we're in a subtree of the current subtree
					t_index = next_branch_index;
				}
			}
			else {
				assert(false, "no sibling branch");
			}
		}
	}
	else if (current_length == 0) {
		r_index = start_index_of_next_branch(tree, t_index);
	}
	else {
		assert(false, "negative length");
	}
	return r_index;
}

mem_index end_index_of_subtree(Buffer* tree, mem_index s_index) {

	mem_index t_index = s_index;
	mem_index e_index = -1;
	mem_index current_length = segment_length(tree, s_index);
	if (current_length > 0) {
		mem_index next_branch_index = -1;
		while (true) {
			next_branch_index = start_index_of_next_branch(tree, t_index);
			if (next_branch_index != -1) {
				mem_index next_segment_length = segment_length(tree, next_branch_index);
				if (next_segment_length == current_length) {
					e_index = next_branch_index - 2;
					break;
				}
				else if (next_segment_length > current_length) {
					e_index = next_branch_index - 2;
					break;
				}
				else {
					//we're in a subtree of the current subtree
					t_index = next_branch_index;
				}
			}
			else {
				//no next branch
				e_index = tree->length - 1;
				break;
			}
		}
	}
	else if (current_length == 0) {
		e_index = s_index;
	}
	else {
		assert(false, "negative length");
	}
	return e_index;
}

/*
 Add a clause to a clause tree. This procedure avoids recursion.
*/
void add_clause(Buffer* tree, Buffer* clause_str, mem_index n, Allocator* alloc) {

	if (tree->length == 0) {
		buffer_clone(tree, clause_str);
		return;
	}

	List status;
	list_init(&status, alloc, sizeof(uint32), n, true);

	//inserted indices must be increasing, and we then look at collapse starting from last one to first one, in that order.

	List checkpoints;
	list_init(&checkpoints, alloc, sizeof(mem_index), n, false);

	//all tree branches have at most n characters.

	// '0' = 48
	// '1' = 49
	mem_index t_index = 0;
	mem_index c_index = 0;
	bool done = false;
	while (!done) { //clause_index, we could also iterate on t_index, it shouldn't make a difference.
		char c = buffer_read_byte(clause_str, c_index);
		//find branch in tree
		char t = buffer_read_byte(tree, t_index);
		if (C_x == t) {
			if (C_x == c) {
				//keep going down 
				list_set(&status, c_index, &S1);

				if (c_index + 1 == n) { //we potentially have to wind back (we could be done going down a 0 subtree, and there may be a 1 subtree to get to).
					//we've reach an end of branch
					//check whether we should wind back c_index and move t_index forward to go down a sibling branch

					mem_index c_i = c_index - 1;
					mem_index length = n - c_i;
					mem_index t_i = t_index - 1;

					while (true) {
						if (buffer_read_byte(tree, t_i) == C_s) {
							t_i--;
						}
						else {
							mem_index current_length = segment_length(tree, t_i);
							if (current_length < length) {
								t_i--;
							}
							else if (current_length == length) {
								//we're done, we found the right node
								break;
							}
							else {
								assert(false, "current_length longer than target length");
							}
						}
					}

					bool need_to_wind_back = false;
					while (true) {
						if (need_to_wind_back || done) break;
						if (S2 == list_read(&status, c_i, uint32)) { 
							//check if there is a sibling branch for t_i, if so, move there, if not, we're done.
							mem_index current_length = segment_length(tree, t_i);
							mem_index tmp_index = t_i;
							if (current_length > 0) {
								mem_index next_branch_index = -1;
								while (true) {
									next_branch_index = start_index_of_next_branch(tree, tmp_index);
									if (next_branch_index != -1) {
										mem_index next_segment_length = segment_length(tree, next_branch_index);
										if (next_segment_length == current_length) {
											//we found that the subtree of the 0 node at t_index has a sibling segment, so we create a checkpoint.
											//move ahead there
											t_index = next_branch_index; //we move past the first node of the sibling branch because it corresponds to the x on the c_string
											c_index = c_i;
											list_set(&status, c_i, &S1);
											need_to_wind_back = true;
											break;
										}
										else if (next_segment_length > current_length) {
											//no sibling branch
											done = true;
											break;
										}
										else {
											//we're still going through subtrees, we keep looking ahead for a sibling branch
											tmp_index = next_branch_index;
										}
									}
									else {
										done = true;
										break; //we reach the end and there's no sibling branch, we're done.
									}
								}
							}
						}
						else {
							c_i--;
							if (c_i == -1) { 
								//we're done.
								done = true;
							}
							else {
								length = n - c_i;
								while (true) {
									if (C_s == buffer_read_byte(tree, t_i)) {
										t_i--;
									}
									else {
										mem_index current_length = segment_length(tree, t_i);
										if (current_length < length) {
											t_i--;
										}
										else if (current_length == length) {
											//we're done, we found the right node
											break;
										}
										else {
											assert(false, "current_length longer than target length");
										}
									}
								}
							}
						}
					}
				}

				if (!done) {
					c_index++;
					t_index++;
					if (c_index == n || t_index == tree->length) {
						done = true;
						break;
					}
				}
				else {
					break;
				}
			}
			else {
				//we need to split the current 'x' on the tree into a '0' and a '1', and go a specific subtree
				//note that past this point we know the two subbranches will be different (at the split point), so we don't need to test equality later at this level

				// ...x[subtree]|[restOfTree]  -> ...0[subtree]|1[subtree]|[restOfTree]
				//we need to split on the current subtree, not all the way for the length of the entire tree, because we could have a sibling branch

				//expand operation.
				//============{A}===============
				//.....t_index
				//.....|
				//.....v
				//.....x[subtree]|[restOfTree]
				//.....0[subtree]|1[subtree]|[restOfTree]

				//1) do a shift right
				//.....x[subtree]|[restOfTree]
				//.....x...........[subtree]|[restOfTree]
				//......*----------> count = subtreeLength + 2
				// 2) copy subtree back
				//.....x...........[subtree]|[restOfTree]
				//.....x[subtree]..[subtree]|[restOfTree]
				// 3) replace x with 0 and write a |1
				//.....x[subtree]..[subtree]|[restOfTree]
				//.....0[subtree]|1[subtree]|[restOfTree]

				mem_index subtree_length = length_of_subtree(tree, t_index + 1);

				//find end of current subtree.
				buffer_shift_right(tree, t_index, subtree_length + 2);
				buffer_write_byte(tree, C_0, t_index);
				buffer_write_bytes(tree, t_index + 1, tree, t_index + subtree_length + 3, subtree_length);
				buffer_write_byte(tree, C_s, t_index + subtree_length + 1);
				buffer_write_byte(tree, C_1, t_index + subtree_length + 2);
				//we continue reprocessing the current index. t_index now points to the 0 node.
			}
		}
		else { //t is 0 or 1
			if (C_x == c) {
				// if t is 1, we know a 0 is missing
				// if t is 0, this could be the only node (no 1), in which case go down the 0 branch and then also add the rest of the clause as a new 1 subtree
				// if t is 0, and there's also a 1 node ahead, we need to go down both branches.
				if (C_1 == t) {
					//we know a '0' subtree is missing
					//we create a new subtree

					//============={B}======================
					//...1[subtree]... 
					//...0[c_substring]|1[subtree]...

					// ...1[subtree]|[rest] -> ...0[rest_of_c_string]|1[subtree]|[rest]

					//.....t_index
					//.....|
					//.....v
					//.....1[subtree]|[restOfTree]
					//.....0[c_substr]|1[subtree]|[restOfTree]

					//1) do a shift right
					//.....x[subtree]|[restOfTree]
					//.....x............[subtree]|[restOfTree]
					//......*-----------> count = c_substrLength + 2
					// 2) copy c_substring
					//.....x............[subtree]|[restOfTree]
					//.....x[c_substr]..[subtree]|[restOfTree]
					// 3) replace x with 0 and write a |1
					//.....x[c_substr]..[subtree]|[restOfTree]
					//.....0[c_substr]|1[subtree]|[restOfTree]

					//we then progress on 0 branch (which is just c_string, so nothing much going on there), and then we'll reach the 1 branch and wind back on c_string 
					//again, subtree doesn't necessarily go all the way to end of tree. We need to find the end position of subtree.
					mem_index subtree_length = length_of_subtree(tree, t_index + 1);

					//find end of current subtree.
					mem_index c_str_length = n - c_index - 1;
					buffer_shift_right(tree, t_index, c_str_length + 2);
					buffer_write_byte(tree, C_0, t_index);
					buffer_write_bytes(tree, t_index + 1, clause_str, c_index + 1, c_str_length);
					buffer_write_byte(tree, C_s, t_index + c_str_length + 1);
					buffer_write_byte(tree, C_1, t_index + c_str_length + 2);
					list_add(&checkpoints, &t_index);
				}
				else if (C_0 == t) {
					//check if there's a '1' subtree ahead 
					list_add(&checkpoints, &t_index);
					if (sibling_branch_index(tree, t_index) == -1) {
						//there is no sibling branch, we need to add it.
						//============{C}=================
						//...t_index
						//...v
						//...0[subtree]|[rest] 
						//...0[subtree]|1[c_substring]|[rest]   
						//
						//1) shift
						//...0[subtree]|[rest]
						//...0[subtree]==============>|[rest]
						//...............n-c_index-1+2
						//2) copy c_str and '|1'
						//...0[subtree]...............|[rest]
						//...0[subtree]|1[c_substring]|[rest]

						mem_index subtree_length = length_of_subtree(tree, t_index + 1);
						//no '0' below because we don't extract '0subtree'
						mem_index c_str_length = n - c_index - 1;
						buffer_shift_right(tree, t_index + subtree_length + 1, c_str_length + 2);
						buffer_write_byte(tree, C_s, t_index + subtree_length + 1);
						buffer_write_byte(tree, C_1, t_index + subtree_length + 2);
						buffer_write_bytes(tree, t_index + subtree_length + 3, clause_str, c_index + 1, c_str_length);
						//no need to add a 2 status here since the sibling branch was missing and is added right from the c_string.
					}
				}

				//we now have both a 0 and a 1 branch.

				//this node will need processing for value 0 and value 1
				list_set(&status, c_index, &S2);
				// progress on the tree in order then move back on the clause_str as soon as we move back up on the main tree (we can deduce this from the length of current segment)
				// we just move forward and then back again, and mark the state of each depth in an array.

				if (c_index + 1 == n) { //we potentially have to wind back (we could be done going down a 0 subtree, and there may be a 1 subtree to get to).
					//we've reach an end of branch
					//check whether we should wind back c_index and move t_index forward to go down a sibling branch

					mem_index c_i = c_index - 1;
					mem_index length = n - c_i;
					mem_index t_i = t_index - 1;

					while (true) {
						if (buffer_read_byte(tree,t_i) == C_s) {
							t_i--;
						}
						else {
							mem_index current_length = segment_length(tree, t_i);
							if (current_length < length) {
								t_i--;
							}
							else if (current_length == length) {
								//we're done, we found the right node
								break;
							}
							else {
								assert(false, "current_length longer than target length");
							}
						}
					}

					bool need_to_wind_back = false;
					while (true) {
						if (need_to_wind_back || done) break;
						if (S2 == list_read(&status, c_i, uint32)) {
							//check if there is a sibling branch for t_i, if so, move there, if not, we're done.
							mem_index current_length = segment_length(tree, t_i);
							mem_index tmp_index = t_i;
							if (current_length > 0) {
								mem_index next_branch_index = -1;
								while (true) {
									next_branch_index = start_index_of_next_branch(tree, tmp_index);
									if (next_branch_index != -1) {
										mem_index next_segment_length = segment_length(tree, next_branch_index);
										if (next_segment_length == current_length) {
											//we found that the subtree of the 0 node at t_index has a sibling segment, so we create a checkpoint.
											//move ahead there
											t_index = next_branch_index; //we move past the first node of the sibling branch because it corresponds to the x on the c_string
											c_index = c_i;
											list_set(&status, c_i, &S1);
											need_to_wind_back = true;
											break;
										}
										else if (next_segment_length > current_length) {
											//no sibling branch
											done = true;
											break;
										}
										else {
											//we're still going through subtrees, we keep looking ahead for a sibling branch
											tmp_index = next_branch_index;
										}
									}
									else {
										done = true;
										break; //we reach the end and there's no sibling branch, we're done.
									}
								}
							}
						}
						else {
							c_i--;
							if (c_i == -1) {
								done = true;
							}
							else {
								length = n - c_i;
								while (true) {
									if (buffer_read_byte(tree, t_i) == C_s) {
										t_i--;
									}
									else {
										mem_index current_length = segment_length(tree, t_i);
										if (current_length < length) {
											t_i--;
										}
										else if (current_length == length) {
											//we're done, we found the right node
											break;
										}
										else {
											assert(false, "current_length longer than target length");
										}
									}
								}
							}
						}
					}
				}

				if (!done) {
					c_index++;
					t_index++;
					if (c_index == n || t_index == tree->length) {
						done = true;
						break;
					}
				}
				else {
					break;
				}
			}
			else {
				if (c == t) {
					// we have 0/0 or 1/1, we keep going down
					list_set(&status, c_index, &S1);
					//only add checkpoint if the node is a zero and there is a sibling branch
					if (C_0 == t) {
						//search for sibling branch
						mem_index current_length = segment_length(tree, t_index);
						mem_index tmp_index = t_index;
						if (current_length > 0) {
							mem_index next_branch_index = -1;
							while (true) {
								next_branch_index = start_index_of_next_branch(tree, tmp_index);
								if (next_branch_index != -1) {
									mem_index next_segment_length = segment_length(tree, next_branch_index);
									if (next_segment_length == current_length) {
										//we found that the subtree of the 0 node at t_index has a sibling segment, so we create a checkpoint.
										list_add(&checkpoints, &t_index);
										break;
									}
									else if (next_segment_length > current_length) {
										//no sibling branch
										break;
									}
									else {
										//we're still going through subtrees, we keep looking ahead for a sibling branch
										tmp_index = next_branch_index;
									}
								}
								else {
									break; //we reach the end and there's no sibling branch
								}
							}
						}
					}

					if (c_index + 1 == n) { //we potentially have to wind back (we could be done going down a 0 subtree, and there may be a 1 subtree to get to).
						//we've reach an end of branch
						//check whether we should wind back c_index and move t_index forward to go down a sibling branch
						mem_index c_i = c_index - 1;
						mem_index length = n - c_i;
						mem_index t_i = t_index - 1;
						while (true) {
							if (buffer_read_byte(tree, t_i) == C_s) {
								t_i--;
							}
							else {
								mem_index current_length = segment_length(tree, t_i);
								if (current_length < length) {
									t_i--;
								}
								else if (current_length == length) {
									//we're done, we found the right node
									break;
								}
								else {
									assert(false, "current_length longer than target length");
								}
							}
						}

						bool need_to_wind_back = false;
						while (true) {
							if (need_to_wind_back || done) break;
							if (S2 == list_read(&status, c_i, uint32)) {
								//check if there is a sibling branch for t_i, if so, move there, if not, we're done.
								mem_index current_length = segment_length(tree, t_i);
								mem_index tmp_index = t_i;
								if (current_length > 0) {
									mem_index next_branch_index = -1;
									while (true) {
										next_branch_index = start_index_of_next_branch(tree, tmp_index);
										if (next_branch_index != -1) {
											mem_index next_segment_length = segment_length(tree, next_branch_index);
											if (next_segment_length == current_length) {
												//we found that the subtree of the 0 node at t_index has a sibling segment, so we create a checkpoint.
												//move ahead there
												t_index = next_branch_index; //we move past the first node of the sibling branch because it corresponds to the x on the c_string
												c_index = c_i;	
												list_set(&status, c_i, &S1);
												need_to_wind_back = true;
												break;
											}
											else if (next_segment_length > current_length) {
												//no sibling branch
												done = true;
												break;
											}
											else {
												//we're still going through subtrees, we keep looking ahead for a sibling branch
												tmp_index = next_branch_index;
											}
										}
										else {
											done = true;
											break; //we reach the end and there's no sibling branch, we're done.
										}
									}
								}
							}
							else {
								c_i--;
								if (c_i == -1) {
									done = true;
								}
								else {
									length = n - c_i;
									while (true) {
										if (buffer_read_byte(tree, t_i) == C_s) {
											t_i--;
										}
										else {
											mem_index current_length = segment_length(tree, t_i);
											if (current_length < length) {
												t_i--;
											}
											else if (current_length == length) {
												//we're done, we found the right node
												break;
											}
											else {
												assert(false, "current_length longer than target length");
											}
										}
									}
								}
							}
						}
					}

					if (!done) {
						c_index++;
						t_index++;
						if (c_index == n || t_index == tree->length) { 
							done = true;
							break;
						}
					}
					else {
						break;
					}
				}
				else if (C_1 == c && C_0 == t) {
					// the tree is already sorted (0 branch first)
					//we should maintain a strict order if we want to compare subtrees as strings, otherwise equal subtree may have differents strings.

					//find if sibling branch exist
					// to find sibling we look for the next segment that's of length L=n-i, making sure we don't come across a segment with length > L (otherwise there's no branch)
					// so we keep looking forward for a segment of length L=n-i, as long as we don't hit a segment of length > L, otherwise we stop and there's no sibling branch

					mem_index current_length = segment_length(tree, t_index);
					mem_index current_t_index = t_index;
					if (current_length > 0) {
						mem_index next_branch_index = -1;
						while (true) {
							next_branch_index = start_index_of_next_branch(tree, t_index);
							if (next_branch_index != -1) {
								mem_index next_segment_length = segment_length(tree, next_branch_index);
								if (next_segment_length == current_length) {
									//we found the sibling segment, we break to keep marching on this one
									list_add(&checkpoints, &current_t_index);
									t_index = next_branch_index;
									break;
								}
								else if (next_segment_length > current_length) {
									//there is no sibling branch, we will need to create a new one and insert the rest of the clause at the end of the current branch (and all its children).
									//nextBranchIndex points to the next subtree, we need to insert there the rest of the clause
									mem_index total_length = tree->length;
									if (C_1 == c) { //tree already in right order, '0' branch first
										//==========={D}==============
										//like {C} above
										list_add(&checkpoints, &current_t_index);
										mem_index c_str_length = n - c_index;
										buffer_shift_right(tree, next_branch_index - 1, c_str_length + 1);
										buffer_write_byte(tree, C_s, next_branch_index - 1);
										buffer_write_bytes(tree, next_branch_index, clause_str, c_index, c_str_length);
										//move the index past the current insertion
										t_index = next_branch_index + n - c_index + 1;
									}
									//wind back 
									while (true) {
										mem_index length = segment_length(tree, t_index);
										c_index = n - length;
										//check status, if status isn't 2, rewind further by looking at the next branch. if status is 2
										if (S2 == list_read(&status, c_index, uint32)) {
											//we're going down a second branch
											list_set(&status, c_index, &S1);
											c_index++;
											t_index++;
											//progress down the new branch
											break;
										}
										else {
											mem_index tlength = tree->length;
											if (tlength > t_index + length) { //there's a next branch
												t_index = t_index + length + 1;
											}
											else {
												done = true;
												break;
											}
										}
									}
									break;
								}
								else {
									//next_segment_length < current_length, we keep going forward
									t_index = next_branch_index;
								}
							}
							else {
								//we're on last branch of the entire tree
								//check if current is 1, we need to swap

								//============={E}================
								if (C_1 == t) { //t = 1, c = 0
									list_add(&checkpoints, &current_t_index);
									mem_index c_str_length = n - c_index;
									buffer_shift_right(tree, t_index, c_str_length + 1);
									buffer_write_bytes(tree, t_index, clause_str, c_index, c_str_length);
									buffer_write_byte(tree, C_s, t_index + c_str_length);
								}
								else { //t = 0, c = 1
									list_add(&checkpoints, &current_t_index);
									//this just appends
									buffer_append_byte(tree, C_s);
									buffer_append_bytes(tree, clause_str, c_index, n - c_index);
								}
								done = true;
								break;
							}
						}
					}
				}
				else if (C_0 == c && C_1 == t) {
					//0 branch is missing, so we know there's no sibling branch for the 1 branch (as we keep things sorted)
					// we can just insert the rest of the c_string
					mem_index total_length = tree->length;

					//'0' subtree appears first
					list_add(&checkpoints, &t_index);

					mem_index c_str_length = n - c_index;
					buffer_shift_right(tree, t_index, c_str_length + 1);
					buffer_write_bytes(tree, t_index, clause_str, c_index, c_str_length);
					buffer_write_byte(tree, C_s, t_index + c_str_length);

					//move the index past the current insertion
					total_length += (n - c_index) + 1;
					mem_index t_length = tree->length;
					t_index = t_index + 2 * (n - c_index) + 2;
					if (t_index < total_length) {
						while (true) {
							mem_index length = segment_length(tree, t_index);
							c_index = n - length;
							//check status, if status isn't 2, rewind further by looking at the next branch. if status is 2
							if (S2 == list_read(&status, c_index, uint32)) {								
								//we're going down a second branch
								list_set(&status, c_index, &S1);
								c_index++;
								t_index++;
								//progress down the new branch
								break;
							}
							else {
								mem_index t_length = tree->length;
								if (t_length > t_index + length) { //there's a next branch
									t_index = t_index + length + 1;
								}
								else {
									done = true;
									break;
								}
							}
						}
					}
					else {
						done = true;
						break;
					}
				}
			}
		}
	}

	// after we insert 10010: 
	//
	//     0 --..
	//     1 ----- 0---------0------0-..
	//     ^       1-..      1-..   1-----0  
	//     a       ^         ^      ^     1
	//             b         c      d     ^
	//                                    e
	// check the subtrees at e, d, c, b, a (progressing and collapsing in that order)

	// use checkpoints to collapse;

	int size = checkpoints.length;
	for (int i = size - 1; i >= 0; i--) {

		//                   
		//   0.....|...|..|.|1......|..|.|
		//   ^               ^ 
		// t_start         s_start  
		//fetch subtree at node and sibling subtree, and compare them.
		//1) return the sibling index s_index corresponding to the start index t_index
		//2) find end index for each
		//              t_end         s_end        
		//                 v            v
		//   0.....|...|..|.|1......|..|.|
		//   ^               ^ 
		// t_start         s_start
		//
		// 3) if lengths are different, don't bother
		// 4) if lengths are equal test for equality

		mem_index t_start = list_read(&checkpoints, i, mem_index);
		
		assert(buffer_read_byte(tree, t_start) == C_0, "checkpoint index should be a 0 node");
		mem_index s_start = start_index_of_sibling_subtree(tree, t_start);
		assert(s_start != -1, "checkpoint sibling should exist");
		assert(buffer_read_byte(tree, s_start) == '1', "checkpoint sibling index should be a 1 node");
		mem_index t_end = s_start - 2;
		
		mem_index t_end2 = end_index_of_subtree(tree, t_start);
		assert(t_end == t_end2, "length check");
		mem_index s_end = end_index_of_subtree(tree, s_start);
		mem_index t_length = t_end - t_start; //length doesn't include the current char
		mem_index s_length = s_end - s_start;
		if (s_length == t_length) {
			if (s_length > 0) {
				//test each subtree for equality
				if (buffer_compare(tree, t_start + 1, tree, s_start + 1, t_length)) {
					
					//we compress
					//
					//<-start>.<-t_tree-->..<-s_tree--><--rest->
					//========0===========|1===========|========
					//........^..........^.^..........^
					//........|..........|.|..........|
					//........|..........|.s_start...s_end
					//........t_start...t_end
					//final:
					//<-start>.<-t_tree--><--rest->
					//========x===========|========
					// operations:
					//1) shift left
					// before
					//<-start>.<-t_tree-->..<-s_tree--><--rest->
					//========0===========|1===========|========
					//....................*<===========*
					// after
					//<-start>.<-t_tree--><--rest->
					//========0===========|========
					//2) replace 0 with x
					// before
					//<-start>.<-t_tree--><--rest->
					//========0===========|========
					// after
					//<-start>.<-t_tree--><--rest->
					//========x===========|========

					buffer_shift_left(tree, s_end + 1, t_length + 2);
					buffer_write_byte(tree, C_x, t_start);
				}
			}
			else {
				//zero length subtrees (we have a 0 leaf and a 1 leaf)
				//<--start->...<-rest->
				//==========0|1|=======
				// t_start  ^ ^ s_end
				// 1) shift left
				//==========0|1|=======
				//...........^<^..shift left by two chars
				//==========0|=======
				//<--start->.<-rest->
				//2) replace 0 with x
				//==========0|=======
				//==========x|=======
				if (t_start > 0) {
					buffer_shift_left(tree, s_end + 1, 2);
					buffer_write_byte(tree, C_x, t_start);
				}
			}
		}
	}
	list_free(&status);
	list_free(&checkpoints);
}

//constants for random generator.
int64 multiplier = 0x5DEECE66DL;
int64 addend = 0xBL;
int64 mask = (1LL << 48) - 1;

struct Rand {
	int64 seed;
};

void rand_set_seed(Rand* r, int64 seed) {
	r->seed = (seed ^ multiplier) & mask;
}
real32 rand_next_real(Rand* r) {
	return 0;
}

int32 rand_next(Rand* r, int bits) {

	r->seed = (r->seed * multiplier + addend) & mask;
	
	return (int32)((uint64)r->seed >> (48 - bits));
}

int32 rand_next_int(Rand* r) {
	return rand_next(r, 32);
}

int32 rand_next_int(Rand* rand, int32 bound) {
	assert(bound > 0, "rand_next_int, bound must be > 0");

	int32 r = rand_next(rand, 31);
	int32 m = bound - 1;
	if ((bound & m) == 0) 
		r = (int32)((bound * (int64)r) >> 31);
	else { 
		for (int u = r;
			u - (r = u % bound) + m < 0;
			u = rand_next(rand, 31));
	}
	return r;
}

int64 rand_next_long(Rand* rand) {
	// it's okay that the bottom word remains signed.
	return ((int64)(rand_next(rand, 32)) << 32) + rand_next(rand, 32);
}

real32 rand_next_real32(Rand* rand) {
	return (real32) (rand_next(rand, 24) / ((int32)(1 << 24)));
}

real64 rand_next_real64(Rand* rand) {
	return (((int64)rand_next(rand, 26) << 27) + rand_next(rand, 27)) / (real64)(1LL << 53);
}

//generate a uniform int between [0,max-1]
uint32 generate_int(int max, Rand* r) {
	real64 p = rand_next_real64(r);
	uint32 i = (uint32)floor(p *(max));
	return i;
}

void test_rand() {

	int64 m = (1LL << 48) - 1;
	int64 m1 = m << 48;

	uint64 seed = 1;
	Rand r;
	rand_set_seed(&r, seed);

	uint32 count = 0;
	uint32 dist[4] = { 0, 0, 0, 0 };
	uint32 total_count = 1000000;
	while (count < total_count) {
		int32 i = rand_next_int(&r, 4);
		dist[i]++;
		count++;
	}

	for (int i = 0; i < 4; i++) {
		printf(" prob(%d) = %f", i, (((real64)dist[i]) / (real64)total_count));
	}
	printf("\n");
}

/*
 generate a random 3-SAT instance, 
 n = number of vars, m = number of clauses, seed = random seed
*/
void generate_random_instance(List* instance, uint32 n, uint32 m, Rand* r, Allocator* alloc) {

	uint32 triplet_count = (n*(n - 1)*(n - 2)) / 6;
	uint32 total_possible_clauses = 8 * triplet_count;
	if (m > total_possible_clauses) m = total_possible_clauses;

	list_clear(instance);

	while (instance->length < m) {

		uint32 i0 = 0, i1 = 0, i2 = 0;
		//pick 3 random var indices
		//pick first var 
		i0 = rand_next_int(r, n);
		//pick second var and sort
		while (true) {
			i1 = rand_next_int(r, n);
			if (i1 != i0) {
				if (i1 < i0) {
					uint32 t = i1;
					i1 = i0;
					i0 = t;
				}
				break;
			}
		}
		//pick third var and sort
		while (true) {
			i2 = rand_next_int(r, n);
			if ((i2 != i0) && (i2 != i1)) {
				if (i2 < i1) {
					if (i2 < i0) {
						//i2 < i0 < i1
						uint32 t = i0;
						i0 = i2;
						i2 = i1;
						i1 = t;
					}
					else {
						//i0 < i2 < i1
						uint32 t = i2;
						i2 = i1;
						i1 = t;
					}
				}
				break;
			}
		}

		//generate the signs
		uint32 signs = rand_next_int(r, 8);
		bool sign0 = ((signs & 1) == 0); //if 0 we pick direct var (a)
		bool sign1 = ((signs & 2) == 0);
		bool sign2 = ((signs & 4) == 0);

		Clause clause;
		clause.i0 = i0;
		clause.i1 = i1;
		clause.i2 = i2;
		clause.b0 = sign0;
		clause.b1 = sign1;
		clause.b2 = sign2;
		bool in_list = false;
		for (uint32 i = 0; i < instance->length; i++) {
			Clause c = list_read(instance, i, Clause);
			if (clause_equal(&clause, &c)) {
				in_list = true;
				break;
			}
		}
		if (!in_list) {
			list_add(instance, &clause);
		}
	}
}

/*
 Finds one possible solution from the provided clause tree.
 Returns false if there are no solution (the clause tree is 'xxx...xx').
 solution_bool is the solution as a list of n true/false variable values.
*/
bool extract_solution(Buffer* tree, uint32 n, List* solution_bool, Allocator* alloc) {

	List solution;
	list_init(&solution, alloc, sizeof(char), n, true);
	uint32 t_index = 0;
	uint32 c_index = 0;

	//only a tree with all x has no solution
	while (c_index < n) {

		char c = buffer_read_byte(tree, t_index);

		if (c == C_x) {
			list_set(&solution, c_index, &C_x);
		} else {
			//node is either a 0 or 1.
			//if node is 0 and all further nodes are x, check if there's a sibling 1 branch (which can't be all x)
			if (c == C_0) {
				mem_index current_length = n - c_index;
				bool all_x_on_0_branch = true;
				mem_index t_i = 1;
				if (t_i == current_length) {
					//we're on last node
					all_x_on_0_branch = false;
				}
				else {
					while (t_i < current_length) {
						if (buffer_read_byte(tree, t_index + t_i) != C_x) {
							all_x_on_0_branch = false;
							break;
						}
						t_i++;
					}
				}
				if (all_x_on_0_branch) {
					uint32 sbi = sibling_branch_index(tree, t_index);
					if (sbi != -1) {
					//there is sibling 1 branch, we switch to it
					//go on 1 branch
						list_set(&solution, c_index, &C_1);
						t_index = sbi;

					}
					else {
						//no sibling branch and all x left
						//switch current node to 1 and add all 1
						list_set(&solution, c_index, &C_1);
						c_index++;
						while (c_index < n) {
							list_set(&solution, c_index, &C_x);
							c_index++;
						}
					}
				} else {
					//not all x left, we progress. if last node, we invert.
					if (c_index == n - 1) {
						//last node, we're on a leaf
						list_set(&solution, c_index, &C_1);
					}
					else {
						list_set(&solution, c_index, &C_0);
					}
				}
			}
			else if (c == '1') {
				if (c_index == n - 1) {
					//last node, we're on a leaf
					list_set(&solution, c_index, &C_0);
				}
				else {
					int current_length = n - c_index;
					bool all_x_on_1_branch = true;
					int t_i = 1;
					if (t_i == current_length) {
						//we're on last node
						all_x_on_1_branch = false;
					}
					else {
						while (t_i < current_length) {
							if (buffer_read_byte(tree, t_index + t_i) != C_x) {
								all_x_on_1_branch = false;
								break;
							}
							t_i++;
						}
					}
					if (all_x_on_1_branch) {
						//we know there's no sibling, since it's a 1 branch, therefore we're done.
						list_set(&solution, c_index, &C_0);
						//we switch to a 1 since all the rest is x.
						c_index++;
						while (c_index < n) {
							list_set(&solution, c_index, &C_x);
							c_index++;
						}
					}
					else {
						//we progress, unless it's the last node then we invert.
						if (c_index == n - 1) {
							//last node, we're on a leaf
							list_set(&solution, c_index, &C_0);
						}
						else {
							list_set(&solution, c_index, &C_1);
						}
					}
				}
			}
		}
		c_index++;
		t_index++;
	}

	//only when solution is all x (i.e. the tree is all x) there is no solution.
	bool has_solution = false;
	for (uint32 i = 0; i < n; i++) {
		if(list_read(&solution, i, char) != C_x) {
			has_solution = true;
			break;
		}
	}
	bool bool_t = true;
	bool bool_f = false;
	if (has_solution) {
		for (uint32 i = 0; i < n; i++) {
			char c = list_read(&solution, i, char);
			if (c == C_x) {
				list_set(solution_bool, i, &bool_t);
			}
			else if (c == C_0) {
				list_set(solution_bool, i, &bool_f);
			}
			else if (c == C_1) {
				list_set(solution_bool, i, &bool_t);
			}
		}
	}
	list_free(&solution);
	return has_solution;
}

void generate_clause_exclusion_string(Buffer* buf, Clause clause, uint32 n) {
	
	buffer_reset(buf);

	for (uint32 i = 0; i < n; i++) {
		if (clause_contains_var(&clause, i)) {
				buffer_append_byte(buf, (clause_find_var_sign(&clause, i) ? '0' : '1')); //it's the exclusion string, so 0 when the variable is true
		}
		else {
			buffer_append_byte(buf, 'x');
		}
	}	
}

/*
 Returns true is the clause tree is entirely filled (in the form 'xxx....xxx').
*/
bool is_full_tree(Buffer* tree, uint32 n) {
	uint32 size = tree->length;
	if (size > n) {
		return false;
	}
	for (uint32 i = 0; i < size; i++) {
		if (buffer_read_byte(tree, i) != C_x) {
			return false;
		}
	}
	return true;
}

bool solve_tree_instance_with_tree_size(List* instance, uint32 n, List* tree_sizes, Buffer* tree, List* solution, Allocator* alloc) {

	Buffer exclusion_string;
	buffer_init(&exclusion_string, n);
	
	for (uint32 i = 0; i < instance->length; i++) {
		
		Clause clause = list_read(instance, i, Clause);
		generate_clause_exclusion_string(&exclusion_string, clause, n);
		add_clause(tree, &exclusion_string, n, alloc);
		if (tree_sizes != NULL) {
			mem_index size = tree->length;
			list_add(tree_sizes, &size);
		}
		if (is_full_tree(tree, n)) { //if the tree is already all x, we're done, no need to check more clauses
			break;
		}
	}
	
	buffer_free(&exclusion_string);

	bool solution_exists = extract_solution(tree, n, solution, alloc);
	return solution_exists;
}

void check_clauses(List* clauses, List* varPos) {
	for (uint32 m = 0; m < clauses->length; m++) {
		Clause clause = list_read(clauses, m, Clause);
		uint32 p0 = list_read(varPos, clause.i0, uint32);
		uint32 p1 = list_read(varPos, clause.i1, uint32);
		uint32 p2 = list_read(varPos, clause.i2, uint32);
		assert(p0 < p1, "");
		assert(p0 < p2, "");
		assert(p1 < p2, "");
	}
}

#define MAX_UINT32 ~((uint32)0)

bool list_same(List* s1, List* s2) {
	//look for equality at a shift (ring like)
	assert(s1->length == s2->length, "list_same non matching length");
	uint32 shift = 0;
	uint32 n = s1->length;
	uint32 v = list_read(s2, 0, uint32);
	for (uint32 i = 0; i < n; i++) {
		uint32 v1 = list_read(s1, i, uint32);
		if (v1 == v) {
			shift = i;
			break;
		}
	}
	for (uint32 i = 0; i < n; i++) {
		uint32 shifted_index = (i + shift + n) % n;
		uint32 v1 = list_read(s1, shifted_index, uint32);
		uint32 v2 = list_read(s2, i, uint32);
		if (v1 != v2) {
			return false;
		}
	}
	return true;
}

bool d_list_contains_list(List* states, List* state) {
	for (uint32 i = 0; i < states->length; i++) {
		List* s = d_list_read(states, i);
		if (list_same(s, state)) {
			return true;
		}
	}
	return false;
}

uint32 get_next_var(uint32 pos, int32 dir, List* var_ring) {

	uint32 next_position = 0;
	if (dir > 0) {
		//move right
		next_position = pos + 1;
		if (next_position == var_ring->length) {
			next_position = 0;
		}
	}
	else if (dir < 0) {
		//move left
		//check edge
		next_position = pos - 1;
		if (next_position == -1) {
			next_position += var_ring->length;
		}
	}
	else if (dir == 0) {
		next_position = pos;
	}
	uint32 var = list_read(var_ring, next_position, uint32);
	return var;
}

void copy_clauses(List* clauses, List* out_clauses) {
	list_clear(out_clauses);
	for (uint32 i = 0; i < clauses->length; i++) {
		Clause c = list_read(clauses, i, Clause);
		list_add(out_clauses, &c);
	}
}

void instance_print(List* instance, List* var_pos, List* var_ring, uint32 n) {
	list_print_clause(instance);
	for (uint32 m = 0; m < instance->length; m++) {
		Clause c = list_read(instance, m, Clause);
		uint32 v0 = list_read(var_pos, c.i0, uint32);
		uint32 v1 = list_read(var_pos, c.i1, uint32);
		uint32 v2 = list_read(var_pos, c.i2, uint32);
		for (uint32 i = 0; i < v0; i++) {
			printf("   ");
		}
		printf("  x");
		for (uint32 i = v0+1; i < v1; i++) {
			printf("---");
		}
		printf("--x");
		for (uint32 i = v1+1; i < v2; i++) {
			printf("---");
		}
		printf("--x\n");
	}
	for (uint32 i = 0; i < n; i++) {
		uint32 p = list_read(var_ring, i, uint32);
		if (i < 10) {
			printf("..%d",p);
		}
		else if (i < 99) {
			printf(".%d", p);
		} 
		else {
			printf("%d", p);
		}
	}
}


uint32 optimize(List* instance, uint32 n, List* in_var_pos, List* lowest_var_pos, List* lowest_var_ring, List* lowest_triplets, Rand* rand, Allocator* alloc) {

	//this one assumes all clauses are 3 and treats them as a triangle, so we optimize
	// only on the longest segment

	uint32 pos_count = n;

	//var_ring[x] is the index of the var at position x, or -1 if it's an empty slot
	List var_ring;
	list_init(&var_ring, alloc, sizeof(uint32), n, true);

	//var_pos[v] is the position of variable v
	List var_pos;
	list_init(&var_pos, alloc, sizeof(uint32), n, true);

	for (uint32 i = 0; i < n; i++) {
		uint32 pos = list_read(in_var_pos, i, uint32);
		list_set(&var_pos, i, &pos);
	}

	//list_print_uint32(&var_pos);

	for (uint32 x = 0; x < pos_count; x++) {
		for (uint32 v = 0; v < n; v++) {
			uint32 pos = list_read(&var_pos, v, uint32);
			if (pos == x) {
				list_set(&var_ring, x, &v);
				break;
			}
		}
	}

	//list_print_uint32(&var_ring);

	List clauses;
	uint32 m = instance->length;
	list_init(&clauses, alloc, sizeof(Clause), m, false);
	//transform clauses into 0 based unsigned lists
	for (uint32 i = 0; i < m; i++) {
		Clause clause = list_read(instance, i, Clause);
		Clause triplet;
		// 0 is always the leftmost element in varPos, 2 is the rightmost, and 1 is the middle one
		uint32 v0 = clause.i0;
		uint32 v1 = clause.i1;
		uint32 v2 = clause.i2;
		bool b0 = clause.b0;
		bool b1 = clause.b1;
		bool b2 = clause.b2;

		uint32 pos0 = list_read(&var_pos, v0, uint32);
		uint32 pos1 = list_read(&var_pos, v1, uint32);
		uint32 pos2 = list_read(&var_pos, v2, uint32);
		if (pos0 < pos1) { // 0 < 1
			if (pos1 < pos2) {  // 0 < 1, 1 < 2
				// 0 < 1 < 2
				triplet.i0 = v0;
				triplet.i1 = v1;
				triplet.i2 = v2;
				triplet.b0 = b0;
				triplet.b1 = b1;
				triplet.b2 = b2;
			}
			else if (pos0 < pos2) { // 0 < 1, 2 < 1, 0 < 2
			 //0 < 2 < 1
				triplet.i0 = v0;
				triplet.i1 = v2;
				triplet.i2 = v1;
				triplet.b0 = b0;
				triplet.b1 = b2;
				triplet.b2 = b1;
			}
			else { // 0 < 1, 2 < 1, 2 < 0
			 //2 < 0 < 1
				triplet.i0 = v2;
				triplet.i1 = v0;
				triplet.i2 = v1;
				triplet.b0 = b2;
				triplet.b1 = b0;
				triplet.b2 = b1;
			}
		}
		else { // 1 < 0
			if (pos0 < pos2) { // 1 < 0, 0 < 2
				// 1 < 0 < 2
				triplet.i0 = v1;
				triplet.i1 = v0;
				triplet.i2 = v2;
				triplet.b0 = b1;
				triplet.b1 = b0;
				triplet.b2 = b2;
			}
			else if (pos1 < pos2) { //1 < 0, 2 < 0, 1 < 2
			 // 1 < 2 < 0
				triplet.i0 = v1;
				triplet.i1 = v2;
				triplet.i2 = v0;
				triplet.b0 = b1;
				triplet.b1 = b2;
				triplet.b2 = b0;
			}
			else { // 1 < 0, 2 < 0, 2 < 1
			 // 2 < 1 < 0
				triplet.i0 = v2;
				triplet.i1 = v1;
				triplet.i2 = v0;
				triplet.b0 = b2;
				triplet.b1 = b1;
				triplet.b2 = b0;
			}
		}
		list_add(&clauses, &triplet);
	}

	check_clauses(&clauses, &var_pos);

	uint32 lowest_total_energy = MAX_UINT32;
	uint32 higher_energy_count = 0;
	
	List lowest_energy_states;
	d_list_init(&lowest_energy_states, alloc, 4);

	List forces;
	list_init(&forces, alloc, sizeof(int32), n, true);
	
	List vars_with_largest_energy_swap;
	list_init(&vars_with_largest_energy_swap, alloc, sizeof(int32), n, false);

	while (true) {
		uint32 total_energy = 0;
		list_set_to_zero(&forces);

		for (uint32 i = 0; i < clauses.length; i++) {
			Clause triplet = list_read(&clauses, i, Clause);

			uint32 v0 = triplet.i0;
			uint32 v1 = triplet.i1;
			uint32 v2 = triplet.i2;

			uint32 pos0 = list_read(&var_pos, v0, uint32);
			uint32 pos1 = list_read(&var_pos, v1, uint32);
			uint32 pos2 = list_read(&var_pos, v2, uint32);

			//distances are always to the right because we always keep 0 has the left most vertex
			int32 d02 = (pos2 - pos0 + pos_count) % pos_count;
			int32 d01 = (pos1 - pos0 + pos_count) % pos_count;
			int32 d12 = (pos2 - pos1 + pos_count) % pos_count;

			//
			//          1
			//         / \
			//       /     \ 
			//     0---------2
			//
			//    when variables move and switch, we update the order so that 0 is always left most
			// so we know that 0---2 is always the longest distance
			//since 1 is always between 0 and 2, there's no point doing anything with 1
			//
			//if 1 and 0 swap, we adjust
			//   1___
			//   |    \____
			//    \         \ 
			//     0---------2
			//
			//   0'___
			//   |    \____
			//    \         \ 
			//     1'---------2

			//compute forces on all three vars
			int32 f0 = +d01 + d02 + list_read(&forces, v0, int32);
			int32 f1 = -d01 + d12 + list_read(&forces, v1, int32);
			int32 f2 = -d12 - d02 + list_read(&forces, v2, int32);
			list_set(&forces, v0, &f0);
			list_set(&forces, v1, &f1);
			list_set(&forces, v2, &f2);

			//list_print_int32(&forces);

			total_energy += 2 * (d01 + d12 + d02);
		}

		//list_print_int32(&forces);

		if (total_energy < lowest_total_energy) {
			higher_energy_count = 0;
			lowest_total_energy = total_energy;
			//save the state
			d_list_free(&lowest_energy_states);
			d_list_init(&lowest_energy_states, alloc, n);
			List* addedState = d_list_create_element_list(&lowest_energy_states, sizeof(uint32), n, true);

			for (uint32 i = 0; i < n; i++) {
				uint32 pos = list_read(&var_pos, i, uint32);
				list_set(lowest_var_pos, i, &pos);
				list_set(addedState, i, &pos);
			}
			//list_print_uint32(added_state);
		}
		else if (total_energy == lowest_total_energy) {
			//check whether the state was already encountered
			if (d_list_contains_list(&lowest_energy_states, &var_pos)) {
				//we stop since we went back to a same low energy state.
				for (uint32 i = 0; i < n; i++) {
					uint32 pos = list_read(&var_pos, i, uint32);
					uint32 var = list_read(&var_ring, i, uint32);
					list_set(lowest_var_pos, i, &pos);
					list_set(lowest_var_ring, i, &var);
				}
				break;
			}
			else {
				//add the state
				List* added_state = d_list_create_element_list(&lowest_energy_states, sizeof(uint32), n, true);
				for (uint32 i = 0; i < n; i++) {
					uint32 pos = list_read(&var_pos, i, uint32);
					uint32 var = list_read(&var_ring, i, uint32);
					list_set(lowest_var_pos, i, &pos);
					list_set(added_state, i, &pos);
					list_set(lowest_var_ring, i, &var);
				}
			}
		}
		else {
			//more energy, we stop after a while
			if (higher_energy_count > 10) {
				break;
			}
			higher_energy_count++;
		}

		//now that we have all the forces on each var, we update their positions
		//move the var with the largest force.

		//look for the swap of 2 vars that gives the largest drop in energy
		// so you add up each var's force (signed) to the force (signed) of the neighbor var is would swap with, 
		// and look for the largest such absolute sum.

		int32 largest_energy_swap = -1;
		for (uint32 i = 0; i < forces.length; i++) {
			int32 f = list_read(&forces, i, int32);
			if (f == 0) continue;
			uint32 pos = list_read(&var_pos, i, uint32);
			uint32 swapped_var = get_next_var(pos, f, &var_ring);
			int32 swapped_var_f = list_read(&forces, swapped_var, int32);
			//energy change is signed, i.e. is can go up or down, but we need to take sign of force into account
			int32 sign = f > 0 ? +1 : -1;
			int32 energy_swap = sign * (f - swapped_var_f);
			if (energy_swap < 0) continue;
			if (energy_swap > largest_energy_swap) {
				largest_energy_swap = energy_swap;
				list_clear(&vars_with_largest_energy_swap);
				list_add(&vars_with_largest_energy_swap, &i);
			}
			else if (abs(energy_swap) == abs(largest_energy_swap)) {
				list_add(&vars_with_largest_energy_swap, &i);
			}
		}

		if (vars_with_largest_energy_swap.length == 0) {
			continue;
		}

		//pick one var
		uint32 rand_index = rand_next_int(rand, vars_with_largest_energy_swap.length);
		uint32 var_with_largest_energy_swap = list_read(&vars_with_largest_energy_swap, rand_index, uint32);

		int32 picked_force = list_read(&forces, var_with_largest_energy_swap, int32);

		//sort clauses by shortest total force in a list
		// find the right var that splits the most the set of clauses (measure crossing count along the ring using directions)
		// put that var at the start by shifting all vars
		// find the areas with the most density of clauses
		// for each var look in the sorted clause list for the first clause it's in and pull that clause out and process it
		// when no more clause, go to next var to the right

		//move the var in the direction of its force
		uint32 current_pos = list_read(&var_pos,var_with_largest_energy_swap,uint32);
		uint32 swap_var = get_next_var(current_pos, picked_force, &var_ring);
		uint32 next_position = list_read(&var_pos,swap_var,uint32);

		//list_print_uint32(&var_ring);

		list_set(&var_ring, next_position, &var_with_largest_energy_swap);

		list_set(&var_ring, current_pos, &swap_var);

		//list_print_uint32(&var_ring);
		//list_print_uint32(&var_pos);

		list_set(&var_pos, var_with_largest_energy_swap, &next_position);

		list_set(&var_pos, swap_var, &current_pos);

		//list_print_uint32(&var_pos);

		for (uint32 i = 0; i < n; i++) {
			uint32 pos = list_read(&var_pos, i, uint32);
			uint32 var = list_read(&var_ring, i, uint32);
			list_set(lowest_var_pos, i, &pos);
			list_set(lowest_var_ring, i, &var);
		}
		//adjust all triplets based on the swap between swapVar and varWithLargestForce
		uint32 var_to_right = swap_var;
		uint32 var_to_left = var_with_largest_energy_swap;
		if (picked_force > 0) {
			var_to_right = var_with_largest_energy_swap;
			var_to_left = swap_var;
		}

		for (uint32 m = 0; m < clauses.length; m++) {

			Clause triplet = list_read(&clauses,m,Clause);

			uint32 v0 = triplet.i0;
			uint32 v1 = triplet.i1;
			uint32 v2 = triplet.i2;
			bool b0 = triplet.b0;
			bool b1 = triplet.b1;
			bool b2 = triplet.b2;

			bool changed = false;

			if (v0 == var_to_right && v1 == var_to_left) {
				triplet.i0 = v1;
				triplet.i1 = v0;
				triplet.b0 = b1;
				triplet.b1 = b0;
				changed = true;
			}
			else if (v1 == var_to_right && v2 == var_to_left) {
				triplet.i1 = v2;
				triplet.i2 = v1;
				triplet.b1 = b2;
				triplet.b2 = b1;
				changed = true;
			}
			if (changed) {
				list_set(&clauses, m, &triplet);
			}
		}
	}
	copy_clauses(&clauses, lowest_triplets);
	check_clauses(lowest_triplets, lowest_var_pos);
	
	//instance_print(lowest_triplets, lowest_var_pos, lowest_var_ring, n);

	list_free(&clauses);
	list_free(&vars_with_largest_energy_swap);
	list_free(&forces);
	d_list_free(&lowest_energy_states);
	list_free(&var_pos);
	list_free(&var_ring);

	return lowest_total_energy;
}

int32 clause_energy(Clause clause, List* varPos, uint32 n) {

	uint32 pos0 = list_read(varPos, clause.i0, uint32);
	uint32 pos1 = list_read(varPos, clause.i1, uint32);
	uint32 pos2 = list_read(varPos, clause.i2, uint32);

	//distances are always to the right because we always keep 0 has the left most vertex
	int32 d02 = (pos2 - pos0 + n) % n;
	int32 d01 = (pos1 - pos0 + n) % n;
	int32 d12 = (pos2 - pos1 + n) % n;

	//
	//          1
	//         / \
	//       /     \ 
	//     0---------2
	//
	//    when variables move and switch, we update the order so that 0 is always left most
	// so we know that 0---2 is always the longest distance
	//since 1 is always between 0 and 2, there's no point doing anything with 1
	//
	//if 1 and 0 swap, we adjust
	//   1___
	//   |    \____
	//    \         \ 
	//     0---------2
	//
	//   0'___
	//   |    \____
	//    \         \ 
	//     1'---------2

	int32 energy = 2 * (d01 + d12 + d02);
	return energy;
}

real32 clause_var_energy(Clause* clause, uint32 v, List* var_pos, uint32 n) {
	uint32 v0 = clause->i0;
	uint32 v1 = clause->i1;
	uint32 v2 = clause->i2;
	uint32 pos0 = list_read(var_pos, v0, uint32);
	uint32 pos1 = list_read(var_pos, v1, uint32);
	uint32 pos2 = list_read(var_pos, v2, uint32);
	int32 d02 = (pos2 - pos0 + n) % n;
	int32 d01 = (pos1 - pos0 + n) % n;
	int32 d12 = (pos2 - pos1 + n) % n;
	real32 energy = 0.0;
	if (v0 == v) {
		energy += 1.0f / ((real32)(d01 + d02));
	}
	else if (v1 == v) {
		energy += 1.0f / ((real32)(d01 + d12));
	}
	else if (v2 == v) {
		energy += 1.0f / ((real32)(d12 + d02));
	}
	return energy;
}

bool var_in_pos_range(uint32 v, List* var_ring, uint32 p0, uint32 p1) {
	for (uint32 p = p0; p <= p1; p++) {
		uint32 var = list_read(var_ring, p, uint32);
		if (var == v) {
			return true;
		}
	}
	return false;
}

int32 energy_in_pos_range(List* clauses, List* clause_energy, List* var_pos, List* var_ring, uint32 n, uint32 p0, uint32 p1) {
	int32 energy = 0;
	for (uint32 m = 0; m < clauses->length; m++) {
		Clause clause = list_read(clauses, m, Clause);
		if (var_in_pos_range(clause.i0, var_ring, p0, p1) && var_in_pos_range(clause.i1, var_ring, p0, p1) && var_in_pos_range(clause.i2, var_ring, p0, p1)) {
			int32 e = list_read(clause_energy, m, int32);
			energy += e;
		}
	}
	return energy;
}

void extract_clauses_in_pos_range(List* clauses, List* clause_energy, List* sorted_clauses, List* var_pos, List* var_ring, uint32 n, uint32 p0, uint32 p1) {
	for (uint32 m = 0; m < clauses->length; m++) {
		Clause clause = list_read(clauses, m, Clause);
		if (var_in_pos_range(clause.i0, var_ring, p0, p1) && var_in_pos_range(clause.i1, var_ring, p0, p1) && var_in_pos_range(clause.i2, var_ring, p0, p1)) {
			list_add(sorted_clauses, &clause);
			list_remove(clauses, m);
			list_remove(clause_energy, m);
			m--;
		}
	}
}

void prioritize_clauses(List* final_sorted_clauses, List* var_pos, List* var_ring, uint32 n, List* clauses, Allocator* alloc) {

	check_clauses(clauses, var_pos);

	List clause_list;
	list_init(&clause_list, alloc, sizeof(Clause), clauses->length, false);

	List clause_energy_list;
	list_init(&clause_energy_list, alloc, sizeof(int32), clauses->length, false);

	for (uint32 m = 0; m < clauses->length; m++) {

		Clause clause = list_read(clauses,m,Clause);

		int32 c_energy = clause_energy(clause, var_pos, n);
		uint32 insertIndex = -1;
		for (uint32 i = 0; i < clause_energy_list.length; i++) {
			int32 energy = list_read(&clause_energy_list, i, int32);
			if (energy >= c_energy) {
				insertIndex = i;
				break;
			}
		}
		if (insertIndex != -1) {

			list_insert(&clause_list, insertIndex, &clause);
			list_insert(&clause_energy_list, insertIndex, &c_energy);
		}
		else {

			list_add(&clause_list, &clause);
			list_add(&clause_energy_list, &c_energy);
		}
	}

	//list_print_int32(&clause_energy_list);

	real32 max_energy = 0.0;
	uint32 max_energy_var = -1;
	uint32 v_index = -1;

	list_clear(final_sorted_clauses);
	for (uint32 v = 0; v < n; v++) {
		real32 var_energy = 0;
		for (uint32 m = 0; m < clause_list.length; m++) {
			Clause clause = list_read(&clause_list, m, Clause);
			if (clause_contains_var(&clause, v)) {
				var_energy += clause_var_energy(&clause, v, var_pos, n);
			}
		}
		if (var_energy > max_energy) {
			max_energy_var = v;
			max_energy = var_energy;
		}
	}
	
	//start at v = max_energy_var
	//build three sets  v,v+1,v+2
	//use var_ring to define the current set, with a min and a max
	uint32 p = list_read(var_pos, max_energy_var, uint32);
	uint32 p0 = p;
	uint32 p1 = p;
	uint32 range = 1;

	while (true) {
		int32 energy_max = 0;
		for (uint32 i = 0; i < range; i++) {
			int32 r0 = p - (range - 1) + i;
			uint32 r1 = p + i;
			if (r0 < 0 || r1 >= n) continue;
			int32 energy = energy_in_pos_range(&clause_list, &clause_energy_list, var_pos, var_ring, n, r0, r1);
			if (energy > energy_max) {
				energy_max = energy;
				p0 = r0;
				p1 = r1;
			}
		}
		if (energy_max > 0) {
			break;
		}
		range++;
	}
	//list_print_int32(&clause_energy_list);
	extract_clauses_in_pos_range(&clause_list, &clause_energy_list, final_sorted_clauses, var_pos, var_ring, n, p0, p1);

	//list_print_int32(&clause_energy_list);
	uint32 ii = 0;
	while (true) {
		if (clause_list.length == 0) break;
		//try expanding range
		//expand range to the left by one
		int32 energy_left = -1;
		int32 energy_right = -1;
		uint32 p00 = p0 - 1;
		if (p00 != -1) {
			energy_left = energy_in_pos_range(&clause_list, &clause_energy_list, var_pos, var_ring, n, p00, p1);
		}
		uint32 p11 = p1 + 1;
		if (p11 < n) {
			energy_right = energy_in_pos_range(&clause_list, &clause_energy_list, var_pos, var_ring, n, p0, p11);
		}
		if (energy_right > energy_left) {
			p1 = p11;
		}
		else {
			p0 = p00;
		}

		extract_clauses_in_pos_range(&clause_list, &clause_energy_list, final_sorted_clauses, var_pos, var_ring, n, p0, p1);
	}
	
	check_clauses(final_sorted_clauses, var_pos);
	list_free(&clause_energy_list);
	list_free(&clause_list);
}

/*
 Optimize a given instance by re-arranging the order of its variables and clauses.
*/
int optimize_instance(List* instance, uint32 n, List* optimized_var_pos, List* optimized_var_ring, List* optimized_instance, uint32 seed, Allocator* alloc) {

	mem_index start_alloc_size = alloc->free_size;

	Rand rand;
	rand_set_seed(&rand, seed);

	List in_var_pos;
	list_init(&in_var_pos, alloc, sizeof(uint32), n, true);

	for (uint32 i = 0; i < n; i++) {
		list_set(&in_var_pos, i, &i); 
	}

	//list_print_uint32(&in_var_pos);

	uint32 count = 0;
	uint32 lowest_total_energy = MAX_UINT32;

	List out_var_pos;
	list_init(&out_var_pos, alloc, sizeof(uint32), n, true);
	List out_var_ring;
	list_init(&out_var_ring, alloc, sizeof(uint32), n, true);
	List out_instance;
	list_init(&out_instance, alloc, sizeof(Clause), n, false);
	List lowest_energy_instance;
	list_init(&lowest_energy_instance, alloc, sizeof(Clause), n, false);

	while (count < 100) { //we rerun the optimization with different initial var list

		//shuffle the variable positions
		for (uint32 i = 0; i < n; i++) {
			int rng = n - i;
			uint32 p = i + rand_next_int(&rand, rng);
			uint32 var_p = list_read(&in_var_pos, p, uint32);
			uint32 var_i = list_read(&in_var_pos, i, uint32);
			list_set(&in_var_pos, p, &var_i);
			list_set(&in_var_pos, i, &var_p);
		}
		//list_print_uint32(&in_var_pos);
		
		uint32 total_energy = optimize(instance, n, &in_var_pos, &out_var_pos, &out_var_ring, &out_instance, &rand, alloc);
		
		if (total_energy < lowest_total_energy) {
			lowest_total_energy = total_energy;
			for (uint32 i = 0; i < n; i++) {
				uint32 pos = list_read(&out_var_pos, i, uint32);
				uint32 var = list_read(&out_var_ring, i, uint32);
				list_set(optimized_var_pos, i, &pos);
				list_set(optimized_var_ring, i, &var);
			}
			copy_clauses(&out_instance, &lowest_energy_instance);
			check_clauses(&lowest_energy_instance, optimized_var_pos);
		}
		
		//reset positions
		for (uint32 i = 0; i < n; i++) {
			list_set(&in_var_pos, i, &i);
		}
		count++;
	}

	prioritize_clauses(optimized_instance, optimized_var_pos, optimized_var_ring, n, &lowest_energy_instance, alloc);

	list_free(&in_var_pos);
	list_free(&out_var_pos);
	list_free(&out_var_ring);
	list_free(&out_instance);
	list_free(&lowest_energy_instance);

	mem_index end_alloc_size = alloc->free_size;

	assert(start_alloc_size == end_alloc_size, "");

	return lowest_total_energy;
}

void transform_solution(List* in_solution, List* out_solution, List* out_var_ring, uint32 n) {
	for (uint32 i = 0; i < n; i++) {
		bool v = list_read(in_solution, i, bool);
		uint32 pos = list_read(out_var_ring, i, uint32);
		list_set(out_solution, pos, &v);
	}
}

/*
 Swap variable order.
*/
void transform_instance(List* var_pos, List* var_ring, uint32 n, List* clauses, List* transformed_clauses) {

	//example
	//var pos:  3, 2, 4, 1, 0, 12, 9, 13, 5, 10, 15, 14, 6, 7, 11, 8
	//var ring: 4, 3, 1, 0, 2, 8, 12, 13, 15, 6, 9, 14, 5, 7, 11, 10
	//clauses:
	//[-2,-9,+10, -0,+12,+15, +2,-8,-15, -15,+6,-11, +13,+6,+9, -8,-13,-9,...

	list_clear(transformed_clauses);
	
	for(uint32 i = 0; i < clauses->length; i++) {
		Clause clause = list_read(clauses, i, Clause);
		uint32 v0 = list_read(var_pos, clause.i0, uint32);
		uint32 v1 = list_read(var_pos, clause.i1, uint32);
		uint32 v2 = list_read(var_pos, clause.i2, uint32);
		clause.i0 = v0;
		clause.i1 = v1;
		clause.i2 = v2;
		list_add(transformed_clauses, &clause);
	}
}

bool test_solution(List* instance, List* solution, uint32 n, bool verbose) {
	
	if (verbose) {
		
		printf("\nSolution = ");
		list_print_bool(solution);
	}
	for (uint32 i = 0; i < instance->length; i++) {
		Clause c = list_read(instance, i , Clause);
		bool b0 = list_read(solution, c.i0, bool);
		bool b1 = list_read(solution, c.i1, bool);
		bool b2 = list_read(solution, c.i2, bool);
		bool s = clause_is_satisfied(&c, b0, b1, b2);

		if (!s) {
			char buffer[100];
			sprintf_s(buffer, "SOLUTION FAILING: clause (%d,%d,%d) failed for values %d,%d,%d \n", (c.b0? c.i0: -1*c.i0), (c.b1 ? c.i1 : -1*c.i1), (c.b2 ? c.i2 : -1*c.i2), b0, b1, b2);
			printf(buffer);
			assert(s, buffer);
			return false;
		}
	}
	if (verbose) printf("SOLUTION PASSES!\n");
	return true;
}

void shuffle_instance_and_check_size(List* instance, List* min_size_instance, List* max_size_instance, List* min_sizes, List* max_sizes, int n, bool shuffle, uint32 shuffle_count, Rand* rand, Allocator* alloc) {

	uint32 length = instance->length;
	uint32 count = 0;
	mem_index max_max_tree_size = 0;
	mem_index min_max_tree_size = 10000000;
	mem_index avg_max_tree_size = 0;

	if (!shuffle) {
		shuffle_count = 1;
	}

	List tree_sizes;
	list_init(&tree_sizes, alloc, sizeof(mem_index), 100, false);
	List volume_sizes;
	list_init(&volume_sizes, alloc, sizeof(real64), 100, false);
	List solution;
	list_init(&solution, alloc, sizeof(bool), n, true);

	Buffer tree;
	buffer_init(&tree, 500000);

	while (count++ < shuffle_count) {

		//shuffle the clauses
		if (shuffle) {
			for (uint32 i = 0; i < length; i++) {
				int rng = length - i;
				int j = i + rand_next_int(rand, rng); //j is between between [i, length - 1]
				//if j != j, swap element i with element i+j
				if (i != j) {
					Clause ci = list_read(instance, i, Clause);
					Clause cj = list_read(instance, j, Clause);
					list_set(instance, i, &cj);
					list_set(instance, j, &ci);
				}
			}
		}

		list_clear(&tree_sizes);
		list_clear(&volume_sizes);

		buffer_reset(&tree);
		list_clear(&tree_sizes);
		bool solution_exists = solve_tree_instance_with_tree_size(instance, n, &tree_sizes, &tree, &solution, alloc);

		//scan for maximum
		mem_index max_size = 0;
		for (uint32 i = 0; i < tree_sizes.length; i++) {
			mem_index size = list_read(&tree_sizes, i, mem_index);
			if (size > max_size) {
				max_size = size;
			}
		}
		avg_max_tree_size += max_size;

		if (max_max_tree_size < max_size) {
			max_max_tree_size = max_size;
			list_clear(max_size_instance);
			list_clear(max_sizes);
			for (uint32 i = 0; i < instance->length; i++) {
				Clause c = list_read(instance, i, Clause);
				list_add(max_size_instance, &c);
				mem_index s = list_read(&tree_sizes, i, mem_index);
				list_add(max_sizes, &s);
			}
		}

		if (max_size < min_max_tree_size) {
			min_max_tree_size = max_size;
			list_clear(min_size_instance);
			list_clear(min_sizes);
			for (uint32 i = 0; i < instance->length; i++) {
				Clause c = list_read(instance, i, Clause);
				list_add(min_size_instance, &c);
				mem_index s = list_read(&tree_sizes, i, mem_index);
				list_add(min_sizes, &s);
			}
		}

		if (solution_exists) {
			bool works = test_solution(instance, &solution, n, false);
			assert(works, "failed solution!");
		}
		printf("max max tree size: %zu\n" + max_max_tree_size);
		printf("min max tree size: %zu\n" + min_max_tree_size);
	}

	avg_max_tree_size = avg_max_tree_size / shuffle_count;

	printf("avg max tree size: %zu" + avg_max_tree_size);

	printf("max sizes: \n");
	list_print_mem_index(max_sizes);
	printf("min sizes: \n");
	list_print_mem_index(min_sizes);

	buffer_free(&tree);
}

/*
 Generates and solves random 3-SAT instances for a given n variables, with m (the number of clauses) in [m_start, m_end], with increment m_inc
 test_count instances are generated for every value of m.
 Gor a given m, the proportion between the count of instances that have a solution vs the total instance count is stored in stats.
*/
void compute_transition_stats(uint32 n, uint32 m_start, uint32 m_end, uint32 m_inc, uint32 test_count, List* stats, Allocator* alloc, bool test_sol) {

	uint32 m1 = m_start;

	List optimized_instance;
	list_init(&optimized_instance, alloc, sizeof(Clause), m_end, false);

	List random_instance;
	list_init(&random_instance, alloc, sizeof(Clause), m_end, false);

	List transformed_instance;
	list_init(&transformed_instance, alloc, sizeof(Clause), m_end, false);

	List in_var_pos;
	list_init(&in_var_pos, alloc, sizeof(uint32), n, true);
	List out_var_pos;
	list_init(&out_var_pos, alloc, sizeof(uint32), n, true);
	List out_var_ring;
	list_init(&out_var_ring, alloc, sizeof(uint32), n, true);
	List solution;
	list_init(&solution, alloc, sizeof(bool), n, true);
	List out_solution;
	list_init(&out_solution, alloc, sizeof(bool), n, true);
	Buffer tree;
	buffer_init(&tree, 500000);

	List tree_sizes;
	list_init(&tree_sizes, alloc, sizeof(mem_index), 100, false);

	List max_sizes;
	list_init(&max_sizes, alloc, sizeof(mem_index), test_count, false);

	Rand r;
	rand_set_seed(&r, 1);

	mem_index max_max_tree_size = 0;
	mem_index min_max_tree_size = 10000000;
	mem_index avg_max_tree_size = 0;

	while (m1 <= m_end) {

		int count = test_count;

		uint32 non_empty_solution_count = 0;
		uint32 no_solution_count = 0;
		uint32 test_index = 0;
		while (count-- > 0) {

			if (count % 10 == 0) {
				printf("m = %d - count = %d\n", m1, count);
			}
			list_set_to_zero(&in_var_pos);
			list_set_to_zero(&out_var_pos);
			list_set_to_zero(&out_var_ring);

			for (uint32 i = 0; i < n; i++) {
				list_set(&in_var_pos, i, &i);
			}

			//list_print_uint32(&in_var_pos);
			
			generate_random_instance(&random_instance, n, m1, &r, alloc);
			
			//instance_print(&random_instance, &in_var_pos, &in_var_pos, n);

			uint32 total_energy = optimize_instance(&random_instance, n, &out_var_pos, &out_var_ring, &optimized_instance, 1, alloc);
			
			//printf("\noutVarPos:\n");
			//list_print_uint32(&out_var_pos);
			//printf("\noutVarRing:\n");
			//list_print_uint32(&out_var_ring);
			//instance_print(&optimized_instance, &out_var_pos, &out_var_ring, n);
			
			buffer_reset(&tree);
			list_clear(&tree_sizes);


			//remap the vars to their new positions, solve on this transformed instance, and then transform the solution back.
			transform_instance(&out_var_pos, &out_var_ring, n, &optimized_instance, &transformed_instance);
			
			bool solution_exists = solve_tree_instance_with_tree_size(&transformed_instance, n, &tree_sizes, &tree, &solution, alloc);
			

			if (solution_exists) {
				if (test_sol) {
					test_solution(&transformed_instance, &solution, n, false);
					transform_solution(&solution, &out_solution, &out_var_ring, n);
					test_solution(&random_instance, &out_solution, n, false);
				}
				non_empty_solution_count++;
			}
			else {
				no_solution_count++;
			}
			
			if (count % 10 == 0) {
				//tree size statistics
				mem_index max_size = 0;
				for (uint32 i = 0; i < tree_sizes.length; i++) {
					mem_index size = list_read(&tree_sizes, i, mem_index);
					if (size > max_size) {
						max_size = size;
					}
				}
				//avg_max_tree_size += max_size;
				list_add(&max_sizes, &max_size);

				if (max_max_tree_size < max_size) {
					max_max_tree_size = max_size;
				}

				if (max_size < min_max_tree_size) {
					min_max_tree_size = max_size;
				}

				//average and ecart type
				real32 average_max_size = 0.0f;
				for (uint32 i = 0; i < max_sizes.length; i++) {
					int32 v = (int32)list_read(&max_sizes, i, uint32);
					average_max_size += v;
				}
				average_max_size = average_max_size / max_sizes.length;

				real32 standard_deviation = 0.0f;
				for (uint32 i = 0; i < max_sizes.length; i++) {
					int32 v = (int32)list_read(&max_sizes, i, uint32);
					standard_deviation += (v - average_max_size)*(v - average_max_size);
				}
				standard_deviation = (real32)(standard_deviation / max_sizes.length);
				standard_deviation = (real32)sqrt(standard_deviation);

				printf("average max tree size: %lf\n", average_max_size);
				printf("max tree size standard deviation: %lf\n", standard_deviation);
				printf("max max tree size: %zu\n", max_max_tree_size);
				printf("min max tree size: %zu\n", min_max_tree_size);
			}
		}

		//max tree size: average and standard deviation
		real32 average_max_size = 0.0f;
		for (uint32 i = 0; i < max_sizes.length; i++) {
			int32 v = (int32)list_read(&max_sizes, i, uint32);
			average_max_size += v;
		}
		average_max_size = average_max_size / max_sizes.length;

		real32 standard_deviation = 0.0f;
		for (uint32 i = 0; i < max_sizes.length; i++) {
			int32 v = (int32)list_read(&max_sizes, i, uint32);
			standard_deviation += (v-average_max_size)*(v-average_max_size);
		}
		standard_deviation = standard_deviation / max_sizes.length;
		standard_deviation = (real32)sqrt(standard_deviation);

		printf("average max tree size: %lf\n", average_max_size);
		printf("max tree size standard deviation: %lf\n", standard_deviation);
		printf("max max tree size: %zu\n", max_max_tree_size);
		printf("min max tree size: %zu\n", min_max_tree_size);

		real32 proportion = (real32)non_empty_solution_count / (real32)(non_empty_solution_count + no_solution_count);
		list_add(stats, &proportion);
		m1 += m_inc;
	}
	list_free(&solution);
	list_free(&out_solution);
	list_free(&out_var_ring);
	list_free(&out_var_pos);
	list_free(&in_var_pos);
	list_free(&random_instance);
	list_free(&transformed_instance);
	list_free(&optimized_instance);

	buffer_free(&tree);
}

/*
 Returns the solution count of clause tree.
*/
uint32 count_tree_solutions(Buffer* tree, uint32 n, Allocator* alloc) {

	List count;
	list_init(&count, alloc, sizeof(uint32), n + 1, true);

	uint32 one = 1;
	list_set(&count, 0, &one);

	uint32 size = tree->length;
	uint32 t = 0;
	uint32 i = 1;
	uint32 filled_volume = 0;
	while (true) {
		
		char c = buffer_read_byte(tree, t);
		if (c == 'x') {
			uint32 v = list_read(&count, i - 1, uint32) << 1;
			list_set(&count, i, &v);
		}
		else if (c == '|') {
			i--;
			filled_volume += list_read(&count, i, uint32);
			//rewind based on number of chars in next subtree (till next '|' or till end of tree)
			uint32 p = t + 1;
			while (true) {
				if (p >= size) {
					break;
				}
				char cc = buffer_read_byte(tree, p);
				if (cc == '|') {
					break;
				}
				i--;
				p++;
			}
		}
		else {
			uint32 v = list_read(&count, i - 1, uint32);
			list_set(&count, i, &v);
		}
		t++;
		if (t >= size) {
			filled_volume += list_read(&count, i, uint32);
			break;
		}
		i++;
	}
	uint32 total_volume = 1 << n;
	uint32 solution_count = total_volume - filled_volume;
	return solution_count;
}

/*
 Transforms an instance expressed as an array of signed var indices (shifted by 1 to avoid +/-0) into clauses
*/
void array_to_instance(List* instance, int32 arr[], uint32 size) {
	
	uint32 i = 0;
	while(i < size) {
		Clause c;
		int32 v0 = arr[i++];
		c.b0 = v0 > 0 ? true : false;
		int32 v1 = arr[i++];
		c.b1 = v1 > 0 ? true : false;
		int32 v2 = arr[i++];
		c.b2 = v2 > 0 ? true : false;

		c.i0 = abs(v0) - 1;
		c.i1 = abs(v1) - 1;
		c.i2 = abs(v2) - 1;
		list_add(instance, &c);
	}
}

/*
 Transforms an instance expressed as clauses into a flat array of signed variable indices (shifted by 1 to avoid +/-0).
*/
void transform_clause_to_list(Clause clause, List* c) {
	assert(c->length == 3, "clause list should be a triplet");
	int32 i0 = (clause.i0 + 1) * (clause.b0 ? +1 : -1);
	int32 i1 = (clause.i1 + 1) * (clause.b1 ? +1 : -1);
	int32 i2 = (clause.i2 + 1) * (clause.b2 ? +1 : -1);
	list_set(c, 0, &i0);
	list_set(c, 1, &i1);
	list_set(c, 2, &i2);
}

/*
 Computes the intersection of two expressions (expressed as signed variable indices, shifted by 1).
*/
bool intersect(List* c1, List* c2, List* intersection, int n) {
	list_clear(intersection);
	if (c1->length == 0) {
		//add c2 to intersections
		for (uint32 i = 0; i < c2->length; i++) {
			int32 v = list_read(c2, i, int32);
			list_add(intersection, &v);
		}
		return true;
	}
	if (c2->length == 0) {
		//add c1 to intersections
		for (uint32 i = 0; i < c1->length; i++) {
			int32 v = list_read(c1, i, int32);
			list_add(intersection, &v);
		}
		return true;
	}

	int i1 = 0;
	int i2 = 0;
	int s1 = c1->length;
	int s2 = c2->length;
	int v1 = 0;
	int v2 = 0;
	
	while (true) {
		if (i1 < s1) {
			v1 = list_read(c1,i1, int32);
		}
		else {
			v1 = n + 2; //value beyond the maximum variable index (starting at 1)
		}
		if (i2 < s2) {
			v2 = list_read(c2, i2, int32);
		}
		else {
			v2 = n + 2; //value beyond the maximum variable index (starting at 1)
		}
		if ((i1 >= s1) && (i2 >= s2)) {
			break;
		}
		if (abs(v1) < abs(v2)) {
			list_add(intersection, &v1);
			i1++;
		}
		else if (abs(v1) > abs(v2)) {
			list_add(intersection, &v2);
			i2++;
		}
		else if (abs(v1) == abs(v2)) {
			//if different sign, no intersection
			if (v1 == v2) {
				list_add(intersection, &v1);
				i1++;
				i2++;
			}
			else {
				return false;
			}
		}
	}
	return true;
}


uint32 total_volume(List* elements, int n) {
	uint32 vol = 0;
	for (uint32 i = 0; i < elements->length; i++) {
		List* element = d_list_read(elements, i);
		vol += 1 << (n - element->length);
	}
	return vol;
}

bool equal_clauses(List* c1, List* c2, uint32 n) {
	
	if (c1->length != c2->length) {
		return false;
	}

	for (uint32 i = 0; i < c1->length; i++) {
		int32 v1 = list_read(c1, i, int32);
		int32 v2 = list_read(c2, i, int32);
		if (v1 != v2) {
			return false;
		}
	}
	return true;
}

void add_element(List* d_list, List* element, int n) {
	List* new_element = d_list_create_element_list(d_list, sizeof(int32), element->length, false);
	for (uint32 i = 0; i < element->length; i++) {
		int32 v = list_read(element, i, int32);
		list_add(new_element, &v);
	}
}

/*
 Counts the number of solution of a given instance (exponential cost).
*/
uint32 solution_count_with_intersections(List* instance, int n, Allocator* alloc) {

	List positives;
	d_list_init(&positives, alloc, 4);
	
	List negatives;
	d_list_init(&negatives, alloc, 4);

	List intersections_against_positives;
	d_list_init(&intersections_against_positives, alloc, 4);

	List intersections_against_negatives;
	d_list_init(&intersections_against_negatives, alloc, 4);

	List clause;
	list_init(&clause, alloc, sizeof(int32), 3, true);

	List intersection;
	list_init(&intersection, alloc, sizeof(int32), 3, false);


	for (uint32 i = 0; i < instance->length; i++) {
		Clause c = list_read(instance, i, Clause);
		transform_clause_to_list(c, &clause);
		list_clear(&intersections_against_positives);
		list_clear(&intersections_against_negatives);
		list_clear(&intersection);
		bool drop_clause = false;
		for (uint32 j = 0; j < positives.length; j++) {
			List* p_clause = d_list_read(&positives, j);
			if (intersect(&clause, p_clause, &intersection, n)) {
				if (equal_clauses(&clause, &intersection, n)) {
					//clause is entirely within another
					drop_clause = true;
					break;
				}
				else {
					add_element(&intersections_against_positives, &intersection, n);
				}
			}
		}
		if (drop_clause) {
			continue;
		}
		list_clear(&intersection);
		for (uint32 j = 0; j < negatives.length; j++) {
			List* p_clause = d_list_read(&negatives, j);
			if (intersect(&clause, p_clause, &intersection, n)) {
				if (equal_clauses(&clause, &intersection, n)) {
					//clause is entirely within another
					drop_clause = true;
					break;
				}
				else {
					add_element(&intersections_against_negatives, &intersection, n);
				}
			}
		}
		if (drop_clause) {
			continue;
		}

		//check whether element does increase the current volume
		uint32 volume_clause = 1 << (n - clause.length);
		uint32 intersections_total_volume = 0;
		for (uint32 i = 0; i < intersections_against_positives.length; i++) {
			List* intersection = d_list_read(&intersections_against_positives, i);
			intersections_total_volume += 1 << (n - intersection->length);
		}
		for (uint32 i = 0; i < intersections_against_negatives.length; i++) {
			List* intersection = d_list_read(&intersections_against_negatives, i);
			intersections_total_volume -= 1 << (n - intersection->length);
		}
		if (volume_clause == intersections_total_volume) {
			//element is already part of the current volume, drop it
			continue;
		}

		//add clause to intersections against negatives so that it ends up in the positives
		add_element(&positives, &clause, n);
		for (uint32 i = 0; i < intersections_against_negatives.length; i++) {
			List* element = d_list_read(&intersections_against_negatives, i);
			add_element(&positives, element, n);
		}
		for (uint32 i = 0; i < intersections_against_positives.length; i++) {
			List* element = d_list_read(&intersections_against_positives, i);
			add_element(&negatives, element, n);
		}
	}

	uint32 positive_volume = total_volume(&positives, n);
	uint32 negative_volume = total_volume(&negatives, n);
	int volume = positive_volume - negative_volume;
	int solution_count = (1 << n) - volume;

	return solution_count;
}

/*
 Computes the sat/unsat transition curve for a simple tiling model.
*/
void compute_3SAT_threshold_approximation_less_simple(uint32 n, Allocator* alloc) {
	//this model considers that tiles belong to a given element and that tiles in the same element cannot occupy the same space.
	uint32 full_vol = 1 << n;
	uint32 element_vol = 1 << (n - 3);
	uint32 start_empty_vol = full_vol - element_vol;
	
	List stats;
	list_init(&stats, alloc, sizeof(real64), start_empty_vol + 2, true);
	list_set_to_zero(&stats);

	List no_sol_prob;
	list_init(&no_sol_prob, alloc, sizeof(real64), 100, false);
	
	List no_sol_ratio;
	list_init(&no_sol_ratio, alloc, sizeof(real64), 100, false);

	List average_volume_ratio;
	list_init(&average_volume_ratio, alloc, sizeof(real64), 100, false);

	real64 prev_ratio = 0.0f;
	real64 zero = 0.0f;
	real64 one = 1.0f;
	list_set(&stats, 0, &zero); //sentinel value
	list_set(&stats, 1, &one); //prob for first element, it all goes to fill the space
	uint32 tile = element_vol; //we start past the first element which is deterministically placed on its own.

	while (true) {
		real64 average_volume = 0.0;
		uint32 tile_count_in_element = tile++ % element_vol;
		real64 f = 1.0f / ((real64)(full_vol - tile_count_in_element));
		real64 previous_down_factor = 1.0f;
		for (uint32 i = start_empty_vol + 1; i >= 1; i--) {
			real64 down_factor = ((real64)(element_vol + i - 2 - tile_count_in_element)) * f;
			uint32 vol = element_vol + i - 1;
			real64 si = list_read(&stats, i, real64);
			real64 si_1 = list_read(&stats, i - 1, real64);
			real64 v = si * previous_down_factor + si_1 * (1.0f - down_factor);
			list_set(&stats, i, &v);
			average_volume += v * vol;
			previous_down_factor = down_factor;
		}

		//store relevant values
		if (tile_count_in_element == element_vol - 1) {
			real64 ratio = ((real64)tile) / ((real64)(n * element_vol)); 
			real64 s = list_read(&stats, start_empty_vol + 1, real64);
			real64 portion_with_solution = 1.0 - s;
			real64 volume_ratio = average_volume / ((real64)full_vol);
			
			if (portion_with_solution < 0.01f) {
				break;
			}
			if (ratio >= prev_ratio + 0.05f) {
				list_add(&no_sol_prob, &portion_with_solution);
				list_add(&no_sol_ratio, &ratio);
				list_add(&average_volume_ratio, &volume_ratio);
				prev_ratio = ratio;
			}
		}
	}

	uint32 size = no_sol_prob.length;

	printf("less simple n = %d\n", n);
	printf("ratios:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&no_sol_ratio, i, real64);
		printf("%lf\n", r);
	}

	printf("probs:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&no_sol_prob, i, real64);
		printf("%lf\n", r);
	}

	printf("vol:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&average_volume_ratio, i, real64);
		printf("%lf\n", r);
	}

	list_free(&no_sol_ratio);
	list_free(&no_sol_prob);
	list_free(&average_volume_ratio);
	list_free(&stats);
}

/*
 Computes the sat/unsat transition curve for the simplest tiling model.
*/
void compute_3SAT_threshold_simple_approximation(uint32 n, Allocator* alloc) {
	//simplest model where probability is same for all tiles, triplet elements are ignored.
	uint32 full_vol = 1 << n;
	uint32 element_vol = 1 << (n - 3);
	
	List stats;
	list_init(&stats, alloc, sizeof(real64), full_vol + 1, true);
	list_set_to_zero(&stats);

	List no_sol_prob;
	list_init(&no_sol_prob, alloc, sizeof(real64), 100, false);

	List no_sol_ratio;
	list_init(&no_sol_ratio, alloc, sizeof(real64), 100, false);

	List average_volume_ratio;
	list_init(&average_volume_ratio, alloc, sizeof(real64), 100, false);

	real64 prev_ratio = 0.0f;
	real64 zero = 0.0f;
	real64 one = 1.0f;
	list_set(&stats, 0, &zero); //sentinel value
	list_set(&stats, 1, &one); //prob for first element, it all goes to fill the space
	
	uint32 tile_count = 1;
	real64 f = 1.0f / ((real64)(full_vol));
	while (true) {
		tile_count++;
		uint32 tile_count_in_element = tile_count % element_vol;
		real64 average_volume = 0.0f;

		real64 previous_down_factor = 1.0f;
		for (uint32 i = full_vol; i >= 1; i--) {
			real64 down_factor = ((real64)(i - 1)) * f;
			real64 si = list_read(&stats, i, real64);
			real64 si_1 = list_read(&stats, i - 1, real64);
			real64 v = si * previous_down_factor + si_1 * (1.0f - down_factor);
			list_set(&stats, i, &v);
			average_volume += v * i;
			previous_down_factor = down_factor;
		}

		//store relevant values
		if (tile_count_in_element == 0) {
			real64 ratio = ((real64)tile_count) / ((real64)(n * element_vol));
			real64 s = list_read(&stats, full_vol, real64);
			real64 portion_with_solution = 1.0f - s;
			real64 volume_ratio = average_volume * f;
			if (portion_with_solution < 0.01f) {
				break;
			}
			if (ratio >= prev_ratio + 0.05f) {
				list_add(&no_sol_prob, &portion_with_solution);
				list_add(&no_sol_ratio, &ratio);
				list_add(&average_volume_ratio, &volume_ratio);
				prev_ratio = ratio;
			}
		}
	}

	uint32 size = no_sol_prob.length;

	printf("n = %d\n", n);
	printf("ratios:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&no_sol_ratio, i, real64);
		printf("%lf\n", r);
	}

	printf("probs:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&no_sol_prob, i, real64);
		printf("%lf\n", r);
	}

	printf("vol:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&average_volume_ratio, i, real64);
		printf("%lf\n", r);
	}

	list_free(&no_sol_ratio);
	list_free(&no_sol_prob);
	list_free(&average_volume_ratio);
	list_free(&stats);
}

void compute_3SAT_threshold_simple_approximation_a(uint32 n, Allocator* alloc) {
	//same as compute_3SAT_threshold_simple_approximation, except we flipped the list so we can process it in increasing order
	uint32 full_vol = 1 << n;
	uint32 element_vol = 1 << (n - 3);

	List stats;
	list_init(&stats, alloc, sizeof(real64), full_vol + 1, true);
	list_set_to_zero(&stats);

	List no_sol_prob;
	list_init(&no_sol_prob, alloc, sizeof(real64), 100, false);

	List no_sol_ratio;
	list_init(&no_sol_ratio, alloc, sizeof(real64), 100, false);

	List average_volume_ratio;
	list_init(&average_volume_ratio, alloc, sizeof(real64), 100, false);

	real64 prev_ratio = 0.0f;
	real64 zero = 0.0f;
	real64 one = 1.0f;
	list_set(&stats, full_vol, &zero); //sentinel value
	list_set(&stats, full_vol - 1, &one); //prob for first element, it all goes to fill the space

	uint32 tile_count = 1;
	real64 f = 1.0f / ((real64)(full_vol));
	while (true) {
		tile_count++;
		uint32 tile_count_in_element = tile_count % element_vol;
		real64 average_volume = 0.0f;
		real64 previous_down_factor = 1.0f;
		real64 si = list_read(&stats, 0, real64); 

		for (uint32 j = 0; j < full_vol; j++) {
			real64 down_factor = ((real64)(full_vol - j - 1)) * f;
			real64 si_1 = list_read(&stats, j + 1, real64); 
			real64 v = si * previous_down_factor + si_1 * (1.0f - down_factor);
			list_set(&stats, j, &v);
			average_volume += v * j;

			previous_down_factor = down_factor;
			si = si_1;
		}

		//list_print_real64(&stats);

		//store relevant values
		if (tile_count_in_element == 0) {
			real64 ratio = ((real64)tile_count) / ((real64)(n * element_vol));
			real64 s = list_read(&stats, 0, real64);
			real64 portion_with_solution = 1.0f - s;
			real64 volume_ratio = average_volume * f;
			
			if (portion_with_solution < 0.01f) {
				break;
			}
			if (ratio >= prev_ratio + 0.05f) {
				list_add(&no_sol_prob, &portion_with_solution);
				list_add(&no_sol_ratio, &ratio);
				list_add(&average_volume_ratio, &volume_ratio);
				prev_ratio = ratio;
			}
		}
	}

	uint32 size = no_sol_prob.length;

	printf("n = %d\n", n);
	printf("ratios:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&no_sol_ratio, i, real64);
		printf("%lf\n", r);
	}

	printf("probs:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&no_sol_prob, i, real64);
		printf("%lf\n", r);
	}

	printf("vol:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&average_volume_ratio, i, real64);
		printf("%lf\n", r);
	}

	list_free(&no_sol_ratio);
	list_free(&no_sol_prob);
	list_free(&average_volume_ratio);
	list_free(&stats);
}


void compute_3SAT_threshold_simple_approximation_m_curve(uint32 n, Allocator* alloc) {
	//compute the M curve
	uint32 full_vol = 1 << n;
	uint32 element_vol = 1 << (n - 3);

	List stats;
	list_init(&stats, alloc, sizeof(real64), full_vol + 1, true);
	list_set_to_zero(&stats);

	List no_sol_prob;
	list_init(&no_sol_prob, alloc, sizeof(real64), 100, false);

	List no_sol_ratio;
	list_init(&no_sol_ratio, alloc, sizeof(real64), 100, false);

	List m_curve;
	list_init(&m_curve, alloc, sizeof(real64), 100, false);

	real64 prev_ratio = 0.0f;
	real64 zero = 0.0f;
	real64 one = 1.0f;
	list_set(&stats, full_vol, &zero); //sentinel value
	list_set(&stats, full_vol - 1, &one); //prob for first element, it all goes to fill the space

	uint32 tile_count = 1;
	real64 f = 1.0f / ((real64)(full_vol));
	while (true) {
		tile_count++;
		uint32 tile_count_in_element = tile_count % element_vol;
		real64 average_volume = 0.0f;

		real64 previous_down_factor = 1.0f;
		real64 si = list_read(&stats, 0, real64);

		for (uint32 i = 0; i < full_vol; i++) {
			real64 down_factor = ((real64)(i + 1)) * f;
			real64 si_1 = list_read(&stats, i + 1, real64);
			real64 v = si * previous_down_factor + si_1 * (1.0f - down_factor);
			list_set(&stats, i, &v);;
			average_volume += v * (full_vol - i);
			previous_down_factor = down_factor;
			si = si_1;
		}
		
		if (tile_count_in_element == 0) {
			real64 ratio = ((real64)tile_count) / ((real64)(n * element_vol));
			printf("ratio = %f\n", ratio);	
			if (ratio >= prev_ratio + 0.05f) {
				real64 s = list_read(&stats, 0, real64);
				real64 portionWithSolution = 1.0f - s;
				real64 volumeRatio = average_volume * f;
				if (portionWithSolution < 0.01f) {
					break;
				}
				list_add(&no_sol_prob, &portionWithSolution);
				list_add(&no_sol_ratio, &ratio);
				list_add(&m_curve, &volumeRatio);
				prev_ratio = ratio;
			}	
		}

		/*
		if (true) {
			real64 ratio = ((real64)tileCount) / ((real64)(n * elementVol));
			//printf("ratio = %f\n", ratio);
			//====================
			real64 s = list_read(&stats, 0, real64);
			real64 portionWithSolution = 1.0f - s;
			real64 volumeRatio = averageVolume * f;
			if (portionWithSolution < 0.01f) {
				break;
			}
			//if (ratio >= 5.545166016f && ratio <= 5.545181274f) { //16

			//====17========
			//if (ratio > 5.5452f) {
			//	break;
			//}
			//if (ratio >= 5.5451f && ratio <= 5.5452f) { 
			
			//if (ratio > 5.6f) {
			//	break;
			//}
			//if (ratio >= 5.5f && ratio <= 5.6f) { 
			if (ratio > 5.8f) {
				break;
			}
			if (ratio >= 5.4f && ratio <= 5.8f) {
				list_add(&noSolProb, &portionWithSolution);
				list_add(&noSolRatio, &ratio);
				list_add(&averageVolumeRatio, &volumeRatio);
				printf("ratio = %f\n", ratio);
				prevRatio = ratio;
			}
		}
		*/
	}

	uint32 size = no_sol_prob.length;

	printf("n = %d\n", n);
	printf("ratios:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&no_sol_ratio, i, real64);
		printf("%.10lf\n", r);
	}

	printf("probs:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&no_sol_prob, i, real64);
		printf("%.10lf\n", r);
	}

	printf("vol:\n=====\n");
	for (uint32 i = 0; i < size; i++) {
		real64 r = list_read(&m_curve, i, real64);
		printf("%.10lf\n", r);
	}

	list_free(&no_sol_ratio);
	list_free(&no_sol_prob);
	list_free(&m_curve);
	list_free(&stats);
}


void test_transition_model() {
	Allocator aa;
	allocator_init(&aa, 10000000, 1);
	uint32 n = 15;
	//compute_3SAT_threshold_approximation_less_simple(n, &aa);
	compute_3SAT_threshold_simple_approximation_m_curve(n, &aa);
}

bool test_optimize_1() {
	Allocator a_0;
	allocator_init(&a_0, 1000000, 1);
	List instance0;
	uint32 n1 = 10;
	list_init(&instance0, &a_0, sizeof(Clause), 10, false);
	//int32 arr0[] = { +3,-5,+8, +1,-4,+10, +2,-3,-6, -2,+4,+7, +1,+7,-9, +2,+5,-8, -3,+6,+7, -1,-2,-6, -3,+4,+5, -1,+6,-9 };
	n1 = 20;
	int32 arr0[] = { -6,-8,9,-5,-7,-15,-9,-10,-14,-3,-4,-15,-3,-10,17,11,-13,20,14,19,20,-3,6,-15,-4,-7,16,1,15,-16,1,13,-18,1,9,18,14,16,-17,-6,10,19,1,5,12,-4,17,19,6,9,-15,-16,17,19,-8,12,13,-3,9,18,6,9,10,-11,16,-20,1,7,-15,2,-6,17,13,-15,16,4,5,-9,1,-8,12,-6,-7,18,4,-9,10,-2,17,20,9,12,-20,6,16,-17,9,-12,17,-3,4,-12,-13,16,-20,-3,4,-15,-2,-16,18,-8,10,-18,-4,-10,-18,7,-9,-12 };
	
	uint32 size = sizeof(arr0)/sizeof(arr0[0]);
	array_to_instance(&instance0, arr0, size);
	list_print_clause(&instance0);
	List vars;
	list_init(&vars, &a_0, sizeof(uint32), n1, true);
	for (uint32 i = 0; i < n1; i++) {
		list_set(&vars, i, &i);
	}
	List out_var_pos;
	list_init(&out_var_pos, &a_0, sizeof(uint32), n1, true);
	List out_var_ring;
	list_init(&out_var_ring, &a_0, sizeof(uint32), n1, true);
	List out_instance;
	list_init(&out_instance, &a_0, sizeof(Clause), 10, false);
	Rand r;
	uint32 totalEnergy = optimize(&instance0, n1, &vars, &out_var_pos, &out_var_ring, &out_instance, &r, &a_0);
	return true;
}

void test_add_clause() {
	
	Buffer b1;
	buffer_init(&b1, 1000);
	Buffer c1;
	buffer_init(&c1, 100);

	Allocator alloc;
	allocator_init(&alloc, 1000, 2);
	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "011");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "100");
	add_clause(&b1, &c1, 3, &alloc);
	char str[1000];
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000|11|100") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "011");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "100");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "111");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100); 
	assert(strcmp(str, "x00|11") == 0, "");
	
	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "xxxx000");
	add_clause(&b1, &c1, 7, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "1x1xx0x");
	add_clause(&b1, &c1, 7, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xxx000|1x0x000|1xx0x") == 0, "");
	
	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "1x1xx0x");
	add_clause(&b1, &c1, 7, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "xxxx000");
	add_clause(&b1, &c1, 7, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xxx000|1x0x000|1xx0x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "111xxxxxx");
	add_clause(&b1, &c1, 9, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "xxx000xxx");
	add_clause(&b1, &c1, 9, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "xxxxxx010");
	add_clause(&b1, &c1, 9, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xx000xxx|1010|1x010|1xx010|10x000xxx|1010|1x010|1xx010|10000xxx|1010|1x010|1xx010|1xxxxxx") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "xxxxx000");
	add_clause(&b1, &c1, 8, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "111xxxxx");
	add_clause(&b1, &c1, 8, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xxxx000|10xxx000|10xx000|1xxxxx") == 0, "");
	
	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000xx");
	add_clause(&b1, &c1, 5, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000xx") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x0x");
	add_clause(&b1, &c1, 5, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000xx|10x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x1x");
	add_clause(&b1, &c1, 5, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xxx") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "010xx");
	add_clause(&b1, &c1, 5, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xxx|10xx") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000x|11|1xx|10xx|101|1x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "x100");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x|11|1xx|1xxx") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "0000|11|101|1001");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "x010");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0000|1x|101|1001|10") == 0, "");
	
	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000|11|110");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "x01");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00x|11|101|10") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "100x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "x00x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "010x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0x0x|100x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "101x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0x0x|10xx") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "011x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x|1xx|10xx") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "111x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x|1xx|10xx|11x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x1");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x|11|1xx|10xx|11x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "11x1");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x|11|1xx|10xx|101|1x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "x100");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x|11|1xx|1xxx") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "x010");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "xxxx") == 0, "");
	
	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00xx|11x|111x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x0");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|11x|111x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x1");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x|11") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "100x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "x00x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x0");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x|10|100x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "01x0");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "000x|10|1x0|100x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "010|101");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "x00");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0x0|10x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "010");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0x0") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "101|10");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "011");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "011|101|10") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "001x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "011x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|11x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "111x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|11x|111x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x0");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|11x|111x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "10x0");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|11x|10x0|11x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x1");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|11x|10x0|11x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "10x1");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "x0xx|11x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "01x1");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|101|1x|10xx|11x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "0x00");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xxx|10xx|11x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "1x00");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xxx|10xx|100|1x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "01");
	add_clause(&b1, &c1, 2, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "0x");
	add_clause(&b1, &c1, 2, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "001|10");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00x|10") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00");
	add_clause(&b1, &c1, 2, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "0x");
	add_clause(&b1, &c1, 2, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000|10");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00x");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00x|10") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "0x1");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00x|11") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00xx|101|1x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "0x00");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xxx") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00xx|11x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "01x1");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|101|1x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "010|101");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "0x1");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "001|1x|101") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000|10|111");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "0x1");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xx|111") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000|111");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "x10");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0x0|11x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "0x0|100");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "0x1");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xx|100") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00xx|11x|10xx|11x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "01x1");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|101|1x|10xx|11x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00xx|11x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "111x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|11x|111x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "x0xx|10x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "011x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0xxx|10xx|10x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "xx0x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "001x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00xx|10x|1x0x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "x00x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "010x");
	add_clause(&b1, &c1, 4, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "0x0x|100x") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "001|10|100");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "111");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "001|10|100|11") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00011|1001");
	add_clause(&b1, &c1, 5, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00100");
	add_clause(&b1, &c1, 5, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00011|100|1001") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "000");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "001");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00x") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "010");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00x|10") == 0, "");
	buffer_reset(&c1);
	buffer_append_string(&c1, "011");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "100");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "101");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "110");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "111");
	add_clause(&b1, &c1, 3, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "xxx") == 0, "");

	buffer_reset(&b1);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00111|1001");
	add_clause(&b1, &c1, 5, &alloc);
	buffer_reset(&c1);
	buffer_append_string(&c1, "00000");
	add_clause(&b1, &c1, 5, &alloc);
	buffer_to_string(&b1, str, 100);
	assert(strcmp(str, "00000|111|1001") == 0, "");
}


void test_transition_stats() {

	Allocator a0;
	allocator_init(&a0, 100000, 2);
	List stats;
	list_init(&stats, &a0, sizeof(real32), 10, false);
	uint32 n = 24; //6;  // 10;  // 15;
	uint32 m1 = (uint32)(4.2 * n);//12; // 20;  // 30;// (uint32)(2 * n);
	uint32 m2 = (uint32)(4.2 * n);//60; // 100; // 150;// (uint32)(10 * n);

	Allocator a1;
	allocator_init(&a1, 2000000, 2);
	compute_transition_stats(n, m1, m2, 2, 10000, &stats, &a1, false);

	list_print_real32(&stats);

	allocator_free(&a1);
	allocator_free(&a0);
}

void test_solution_count_with_intersections() {
	Allocator a_0;
	allocator_init(&a_0, 20000000, 1);
	List instance0;
	uint32 n = 10;
	list_init(&instance0, &a_0, sizeof(Clause), 10, false);
	int32 arr0[] = { +3,-5,+8, +1,-4,+10, +2,-3,-6, -2,+4,+7, +1,+7,-9, +2,+5,-8, -3,+6,+7, -1,-2,-6, -3,+4,+5, -1,+6,-9 };
	uint32 size = sizeof(arr0) / sizeof(arr0[0]);
	array_to_instance(&instance0, arr0, size);
	Buffer tree;
	buffer_init(&tree, 500000);
	List solution;
	list_init(&solution, &a_0, sizeof(bool), n, true);
	bool solution_exists = solve_tree_instance_with_tree_size(&instance0, n, NULL, &tree, &solution, &a_0);
	uint32 sol_count_1 = count_tree_solutions(&tree, n, &a_0);
	uint32 sol_count_2 = solution_count_with_intersections(&instance0, n, &a_0);
	assert(sol_count_1 == sol_count_2, "");
}

int main(int ArgCount, char **Args)
{
	test_buffer();
	clause_test();
	test_rand();
	//test_transition_model();
	test_add_clause();

	//test_optimize_1();
	//test_solution_count_with_intersections();
	test_transition_stats();
	
}
