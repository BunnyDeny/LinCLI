#ifndef _INIT_D__H__
#define _INIT_D__H__

struct init_d {
	char name[32];
	void *_private;
	void (*_init_entry)(void *);
} __attribute__((aligned(sizeof(long))));

#define _EXPORT_INIT_SYMBOL(obj, private, init_entry)                    \
	static struct init_d init_d_##obj __attribute__((                \
		used, section(".my_init_d"), aligned(sizeof(long)))) = { \
		.name = #obj,                                            \
		._private = private,                                     \
		._init_entry = init_entry,                               \
	}

extern struct init_d _init_d_start;
extern struct init_d _init_d_end;

#define _FOR_EACH_INIT_D(_start, _end, _init_d) \
	for ((_init_d) = (_start); (_init_d) < (_end); (_init_d)++)

#endif
