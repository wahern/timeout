struct benchops {
	void *(*init)(struct timeout *, size_t, int);
	void (*add)(void *, struct timeout *, timeout_t);
	void (*del)(void *, struct timeout *);
	struct timeout *(*get)(void *);
	void (*update)(void *, timeout_t);
	void (*check)(void *);
	int (*empty)(void *);
	void (*destroy)(void *);
}; /* struct benchops() */
