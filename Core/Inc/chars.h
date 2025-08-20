#ifndef INC_CHARS_H_
#define INC_CHARS_H_

static inline char lower(char ch) {
	return (ch >= 'A' && ch <= 'Z') ? ch + 32 : ch;
}

#endif /* INC_CHARS_H_ */