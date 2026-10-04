typedef struct {
    int operation;      // 0 = ALLOCATE, 1 = DEALLOCATE
    int size;           // Memory size in KB (0 for DEALLOCATE)
    int allocation_id;  // Unique ID for tracking
} memory_event_t;