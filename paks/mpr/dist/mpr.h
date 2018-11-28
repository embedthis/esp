/*
    mpr.h -- Header for the Multithreaded Portable Runtime (MPR).

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/**
    @file mpr.h
    The Multithreaded Portable Runtime (MPR) is a portable runtime library for embedded applications.
    @description The MPR provides management for logging, error handling, events, files, http, memory, ssl,
    sockets, strings, xml parsing, and date/time functions. It also provides a foundation of safe routines for secure
    programming, that help to prevent buffer overflows and other security threats. The MPR is a library and a C API that can
    be used in both C and C++ programs.
    \n\n
    The MPR uses a set extended typedefs for common types. These include: bool, cchar, cvoid, uchar, short, ushort,
    int, uint, long, ulong, int32, uint32, int64, uint64, float, and double. The cchar type is a const char, cvoid is
    const void. Several types have "u" prefixes to denote unsigned qualifiers.
    \n\n
    The MPR includes a memory allocator and generational garbage collector. The allocator is a fast, immediate
    coalescing allocator that will return memory back to the O/S if not required. It is optimized for frequent
    allocations of small blocks (< 4K) and uses a scheme of free queues for fast allocation.
    \n\n
    The MPR provides a high-performance thread-pool to share threads as required to service clients.
    When a client request arrives, the MPR allocates an event queue called a dispatcher. This dispatcher then serializes
    all activity for the request so that it essentially runs single-threaded  This simplifies the code as most
    interactions do not need to be lock protected. When a request has activity, it borrows a thread from the thread pool,
    does its work and then returns the thread to the thread pool. This all happens very quickly, so a small pool of
    threads are effectivelyshared over many requests. Thread are free to block if required, but typically non-blocking
    patterns are more economical. If you have non-MPR threads that need to call into the MPR, you must synchronize
    such calls via #mprCreateEvent.
 */

#ifndef _h_MPR
#define _h_MPR 1

/********************************** Includes **********************************/

#include "me.h"
#include "osdep.h"

/*********************************** Defines **********************************/

#if DOXYGEN
    /** Argument for sockets */
    typedef int Socket;

    /** Unsigned integral type. Equivalent in size to void* */
    typedef long size_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct  tm;
struct  Mpr;
struct  MprMem;
struct  MprBuf;
struct  MprCmd;
struct  MprCache;
struct  MprCond;
struct  MprDispatcher;
struct  MprEvent;
struct  MprEventService;
struct  MprFile;
struct  MprFileSystem;
struct  MprHash;
struct  MprHeap;
struct  MprJson;
struct  MprJsonParser;
struct  MprList;
struct  MprKey;
struct  MprModule;
struct  MprMutex;
struct  MprOsService;
struct  MprPath;
struct  MprSignal;
struct  MprSocket;
struct  MprSocketService;
struct  MprSsl;
struct  MprThread;
struct  MprThreadService;
struct  MprWaitService;
struct  MprWaitHandler;
struct  MprWorker;
struct  MprWorkerService;
struct  MprXml;

#ifndef ME_MPR_LOGGING
    #define ME_MPR_LOGGING 1            /**< Default for logging is "on" */
#endif
#ifndef ME_MPR_DEBUG_LOGGING
    #if ME_DEBUG
        #define ME_MPR_DEBUG_LOGGING 1
    #else
        #define ME_MPR_DEBUG_LOGGING 0
    #endif
#endif
#ifndef ME_MPR_TEST
    #define ME_MPR_TEST 1
#endif

#ifndef ME_MPR_MAX_PASSWORD
    #define ME_MPR_MAX_PASSWORD 256    /**< Max password length */
#endif

/*
    Select wakeup port. Port can be any free port number. If this is not free, the MPR will use the next free port.
 */
#ifndef ME_WAKEUP_PORT
    #define ME_WAKEUP_PORT      9473
#endif
#define MPR_FD_MIN              32

/*
    Signal sent on Unix to break out of a select call.
 */
#define MPR_WAIT_SIGNAL         (SIGUSR2)

/*
    Socket event message
 */
#define MPR_SOCKET_MESSAGE      (WM_USER + 32)

/*
    Priorities
 */
#define MPR_BACKGROUND_PRIORITY 15          /**< May only get CPU if idle */
#define MPR_LOW_PRIORITY        25
#define MPR_NORMAL_PRIORITY     50          /**< Normal (default) priority */
#define MPR_HIGH_PRIORITY       75
#define MPR_CRITICAL_PRIORITY   99          /**< May not yield */

#define MPR_EVENT_PRIORITY      50          /**< Normal priority */
#define MPR_WORKER_PRIORITY     50          /**< Normal priority */
#define MPR_REQUEST_PRIORITY    50          /**< Normal priority */

/*
    Timeouts
 */
#define MPR_TIMEOUT_PRUNER      120000      /**< Time between worker thread pruner runs (2 min) */
#define MPR_TIMEOUT_WORKER      60000       /**< Prune worker that has been idle for 1 min */
#define MPR_TIMEOUT_START_TASK  10000       /**< Time to start tasks running */
#define MPR_TIMEOUT_STOP        30000       /**< Default wait when stopping resources (30 sec) */
#define MPR_TIMEOUT_STOP_TASK   10000       /**< Time to stop or reap tasks (vxworks) */
#define MPR_TIMEOUT_LINGER      2000        /**< Close socket linger timeout */
#define MPR_TIMEOUT_GC_SYNC     100         /**< Short wait period for threads to synchronize */
#define MPR_TIMEOUT_NO_BUSY     1000        /**< Wait period to minimize CPU drain */
#define MPR_TIMEOUT_NAP         20          /**< Short pause */

#define MPR_MAX_TIMEOUT         MAXINT64

/*
    Default thread counts
 */
#define MPR_DEFAULT_MIN_THREADS 0           /**< Default min threads */
#define MPR_DEFAULT_MAX_THREADS 5           /**< Default max threads */

/*
    Debug control
 */
#define MPR_MAX_BLOCKED_LOCKS   100         /* Max threads blocked on lock */
#define MPR_MAX_RECURSION       15          /* Max recursion with one thread */
#define MPR_MAX_LOCKS           512         /* Total lock count max */
#define MPR_MAX_LOCK_TIME       (60 * 1000) /* Time in msec to hold a lock */

#define MPR_TIMER_TOLERANCE     2           /* Used in timer calculations */
#define MPR_CMD_TIMER_PERIOD    5000        /* Check for expired commands */

/**
    Events
 */
#define MPR_EVENT_TIME_SLICE    20          /* 20 msec */

/**
    Maximum number of files to close when forking
 */
#define MPR_MAX_FILE            256

/*
    Event notification mechanisms
 */
#define MPR_EVENT_ASYNC         1           /**< Windows async select */
#define MPR_EVENT_EPOLL         2           /**< epoll_wait */
#define MPR_EVENT_KQUEUE        3           /**< BSD kqueue */
#define MPR_EVENT_SELECT        4           /**< traditional select() */

#ifndef ME_EVENT_NOTIFIER
    #if MACOSX || SOLARIS
        #define ME_EVENT_NOTIFIER MPR_EVENT_KQUEUE
    #elif WINDOWS
        #define ME_EVENT_NOTIFIER MPR_EVENT_ASYNC
    #elif VXWORKS
        #define ME_EVENT_NOTIFIER MPR_EVENT_SELECT
    #elif (LINUX || ME_BSD_LIKE)
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
            #define ME_EVENT_NOTIFIER MPR_EVENT_EPOLL
        #else
            #define ME_EVENT_NOTIFIER MPR_EVENT_SELECT
        #endif
    #else
        #define ME_EVENT_NOTIFIER MPR_EVENT_SELECT
    #endif
#endif

/**
    Maximum number of notifier events
 */
#ifndef ME_MAX_EVENTS
    #define ME_MAX_EVENTS      32
#endif

/*
    Garbage collector tuning
 */
#define MPR_MIN_TIME_FOR_GC     2       /**< Wait till 2 milliseconds of idle time possible */

/************************************ Error Codes *****************************/

/* Prevent collisions with 3rd party software */
#undef UNUSED

/*
    Standard errors
 */
#define MPR_ERR_OK                      0       /**< Success */
#define MPR_ERR_BASE                    -1      /**< Base error code */
#define MPR_ERR                         -1      /**< Default error code */
#define MPR_ERR_ABORTED                 -2      /**< Action aborted */
#define MPR_ERR_ALREADY_EXISTS          -3      /**< Item already exists */
#define MPR_ERR_BAD_ARGS                -4      /**< Bad arguments or paramaeters */
#define MPR_ERR_BAD_FORMAT              -5      /**< Bad input format */
#define MPR_ERR_BAD_HANDLE              -6      /**< Bad file handle */
#define MPR_ERR_BAD_STATE               -7      /**< Module is in a bad state */
#define MPR_ERR_BAD_SYNTAX              -8      /**< Input has bad syntax */
#define MPR_ERR_BAD_TYPE                -9      /**< Bad object type */
#define MPR_ERR_BAD_VALUE               -10     /**< Bad or unexpected value */
#define MPR_ERR_BUSY                    -11     /**< Resource is busy */
#define MPR_ERR_CANT_ACCESS             -12     /**< Cannot access the file or resource */
#define MPR_ERR_CANT_ALLOCATE           -13     /**< Cannot allocate resource */
#define MPR_ERR_CANT_COMPLETE           -14     /**< Operation cannot complete */
#define MPR_ERR_CANT_CONNECT            -15     /**< Cannot connect to network or resource */
#define MPR_ERR_CANT_CREATE             -16     /**< Cannot create the file or resource */
#define MPR_ERR_CANT_DELETE             -17     /**< Cannot delete the resource */
#define MPR_ERR_CANT_FIND               -18     /**< Cannot find resource */
#define MPR_ERR_CANT_INITIALIZE         -19     /**< Cannot initialize resource */
#define MPR_ERR_CANT_LOAD               -20     /**< Cannot load the resource */
#define MPR_ERR_CANT_OPEN               -21     /**< Cannot open the file or resource */
#define MPR_ERR_CANT_READ               -22     /**< Cannot read from the file or resource */
#define MPR_ERR_CANT_WRITE              -23     /**< Cannot write to the file or resource */
#define MPR_ERR_DELETED                 -24     /**< Resource has been deleted */
#define MPR_ERR_MEMORY                  -25     /**< Memory allocation error */
#define MPR_ERR_NETWORK                 -26     /**< Underlying network error */
#define MPR_ERR_NOT_INITIALIZED         -27     /**< Module or resource is not initialized */
#define MPR_ERR_NOT_READY               -28     /**< Resource is not ready */
#define MPR_ERR_READ_ONLY               -29     /**< The operation timed out */
#define MPR_ERR_TIMEOUT                 -30     /**< Operation exceeded specified time allowed */
#define MPR_ERR_TOO_MANY                -31     /**< Too many requests or resources */
#define MPR_ERR_WONT_FIT                -32     /**< Requested operation won't fit in available space */
#define MPR_ERR_WOULD_BLOCK             -33     /**< Blocking operation would block */
#define MPR_ERR_MAX                     -34

/*
    Error line number information.
 */
#define MPR_LINE(s)         #s
#define MPR_LINE2(s)        MPR_LINE(s)
#define MPR_LINE3           MPR_LINE2(__LINE__)
#define MPR_LOC             __FILE__ ":" MPR_LINE3
#define MPR_NAME(msg)       msg "@" MPR_LOC

#define MPR_STRINGIFY(s)    #s

/*
    Convenience define to declare a main program entry point that works for Windows, VxWorks and Unix
 */
#if VXWORKS
    #define MAIN(name, _argc, _argv, _envp)  \
        static int innerMain(int argc, char **argv, char **envp); \
        int name(char *arg0, ...) { \
            va_list args; \
            char *argp, *largv[ME_MAX_ARGC]; \
            int largc = 0; \
            va_start(args, arg0); \
            largv[largc++] = #name; \
            if (arg0) { \
                largv[largc++] = arg0; \
            } \
            for (argp = va_arg(args, char*); argp && largc < ME_MAX_ARGC; argp = va_arg(args, char*)) { \
                largv[largc++] = argp; \
            } \
            return innerMain(largc, largv, NULL); \
        } \
        static int innerMain(_argc, _argv, _envp)
#elif ME_WIN_LIKE
    #define MAIN(name, _argc, _argv, _envp)  \
        APIENTRY WinMain(HINSTANCE inst, HINSTANCE junk, char *command, int junk2) { \
            PUBLIC int main(); \
            char *largv[ME_MAX_ARGC]; \
            int largc; \
            largc = mprParseArgs(command, &largv[1], ME_MAX_ARGC - 1); \
            largv[0] = #name; \
            main(largc, largv, NULL); \
        } \
        int main(_argc, _argv, _envp)
#else
    #define MAIN(name, _argc, _argv, _envp) int main(_argc, _argv, _envp)
#endif

#if ME_UNIX_LIKE
    typedef pthread_t   MprOsThread;
#elif ME_64
    typedef int64       MprOsThread;
#else
    typedef int         MprOsThread;
#endif

/**
    Elapsed time data type. Stores time in milliseconds from some arbitrary start epoch.
 */
typedef Ticks MprTicks;

/************************************** Debug *********************************/
/**
    Trigger a breakpoint.
    @description This routine is invoked for assertion errors from #mprAssert and errors from #mprError.
        It is useful in debuggers as breakpoint location for detecting errors.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC void mprBreakpoint(void);

#undef assert
#if DOXYGEN
/**
    Assert that a condition is true
    @param cond Boolean result of a conditional test
    @ingroup Mpr
    @stability Stable
 */
PUBLIC void assert(bool cond);
#elif ME_MPR_DEBUG_LOGGING
    #undef assert
    #define assert(C)   if (C) ; else mprAssert(MPR_LOC, #C)
#else
    #undef assert
    #define assert(C)   if (1) ; else {}
#endif

/*********************************** Thread Sync ******************************/
/**
    Multithreaded Synchronization Services
    @see MprCond MprMutex MprSpin mprAtomicAdd mprAtomicAdd64 mprAtomicBarrier mprAtomicCas mprAtomicExchange
        mprAtomicListInsert mprCreateCond mprCreateLock mprCreateSpinLock mprGlobalLock mprGlobalUnlock mprInitLock
        mprInitSpinLock mprLock mprResetCond mprSignalCond mprSignalMultiCond mprSpinLock mprSpinUnlock mprTryLock
        mprTrySpinLock mprUnlock mprWaitForCond mprWaitForMultiCond
    @stability Internal.
    @defgroup MprSync MprSync
 */
typedef struct MprSync { int dummy; } MprSync;

#ifndef ME_MPR_SPIN_COUNT
    #define ME_MPR_SPIN_COUNT 1500 /* Windows lock spin count */
#endif

/**
    Condition variable for single and multi-thread synchronization. Condition variables can be used to coordinate
    activities. These variables are level triggered in that a condition can be signalled prior to another thread
    waiting. Condition variables can be used when single threaded but mprServiceEvents should be called to pump events
    until another callback invokes mprWaitForCond.
    @ingroup MprSync
    @stability Internal.
 */
typedef struct MprCond {
    #if ME_UNIX_LIKE
        pthread_cond_t cv;          /**< Unix pthreads condition variable */
    #elif ME_WIN_LIKE
        HANDLE cv;                  /**< Windows event handle */
    #elif VXWORKS
        SEM_ID cv;                  /**< Condition variable */
    #else
        #warning "Unsupported OS in MprCond definition in mpr.h"
    #endif
    struct MprMutex *mutex;         /**< Thread synchronization mutex */
    volatile int triggered;         /**< Value of the condition */
} MprCond;


/**
    Create a condition lock variable.
    @description This call creates a condition variable object that can be used in #mprWaitForCond and #mprSignalCond calls.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC MprCond *mprCreateCond(void);

/**
    Reset a condition variable. This sets the condition variable to the unsignalled condition.
    @param cond Condition variable object created via #mprCreateCond
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC void mprResetCond(MprCond *cond);

/**
    Wait for a condition lock variable.
    @description Wait for a condition lock variable to be signaled. If the condition is signaled before the timeout
        expires, this call will reset the condition variable and return. This way, it automatically resets the variable
        for future waiters.
    @param cond Condition variable object created via #mprCreateCond
    @param timeout Time in milliseconds to wait for the condition variable to be signaled.
    @return Zero if the event was signalled. Returns < 0 for a timeout.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC int mprWaitForCond(MprCond *cond, MprTicks timeout);

/**
    Signal a condition lock variable.
    @description Signal a condition variable and set it to the \a triggered status. Existing or future caller of
        #mprWaitForCond will be awakened. The condition variable will be automatically reset when the waiter awakes.
        Should only be used for single waiters. Use mprSignalMultiCond for use with multiple waiters.
        \n\n
        This API (like nearly all MPR APIs) must only be used by MPR threads and not by non-MPR (foreign) threads.
        If you need to synchronize active of MPR threads with non-MPR threads, use #mprCreateEvent which can be called from
        foreign threads.
    @param cond Condition variable object created via #mprCreateCond
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC void mprSignalCond(MprCond *cond);

/**
    Signal a condition lock variable for use with multiple waiters.
    @description Signal a condition variable and set it to the \a triggered status. Existing or future callers of
        #mprWaitForCond will be awakened. The conditional variable will not be automatically reset and must be reset
        manually via mprResetCond.
    @param cond Condition variable object created via #mprCreateCond
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC void mprSignalMultiCond(MprCond *cond);

/**
    Wait for a condition lock variable for use with multiple waiters.
    @description Wait for a condition lock variable to be signaled. Multiple waiters are supported and the
        condition variable must be manually reset via mprResetCond. The condition may signaled before calling
        mprWaitForMultiCond.
    @param cond Condition variable object created via #mprCreateCond
    @param timeout Time in milliseconds to wait for the condition variable to be signaled.
    @return Zero if the event was signalled. Returns < 0 for a timeout.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC int mprWaitForMultiCond(MprCond *cond, MprTicks timeout);

/**
    Multithreading lock control structure
    @description MprMutex is used for multithread locking in multithreaded applications.
    @ingroup MprSync
    @stability Internal.
 */
typedef struct MprMutex {
    #if ME_WIN_LIKE
        CRITICAL_SECTION cs;            /**< Internal mutex critical section */
        bool             freed;         /**< Mutex has been destroyed */
    #elif VXWORKS
        SEM_ID           cs;
    #elif ME_UNIX_LIKE
        pthread_mutex_t  cs;
    #else
        #warning "Unsupported OS in MprMutex definition in mpr.h"
    #endif
#if ME_DEBUG
        MprOsThread owner;
#endif
} MprMutex;


/**
    Multithreading spin lock control structure
    @description MprSpin is used for multithread locking in multithreaded applications.
    @ingroup MprSync
    @stability Internal.
 */
typedef struct MprSpin {
    #if USE_MPR_LOCK
        MprMutex                cs;
    #elif ME_WIN_LIKE
        CRITICAL_SECTION        cs;            /**< Internal mutex critical section */
        bool                    freed;         /**< Mutex has been destroyed */
    #elif VXWORKS
        SEM_ID                  cs;
    #elif ME_UNIX_LIKE
        #if ME_COMPILER_HAS_SPINLOCK
            pthread_spinlock_t  cs;
        #else
            pthread_mutex_t     cs;
        #endif
    #else
        #warning "Unsupported OS in MprSpin definition in mpr.h"
    #endif
#if ME_DEBUG
        MprOsThread             owner;
#endif
} MprSpin;


#undef lock
#undef unlock
#undef spinlock
#undef spinunlock
#define lock(arg)       if (arg && (arg)->mutex) mprLock((arg)->mutex)
#define unlock(arg)     if (arg && (arg)->mutex) mprUnlock((arg)->mutex)
#define spinlock(arg)   if (arg) mprSpinLock((arg)->spin)
#define spinunlock(arg) if (arg) mprSpinUnlock((arg)->spin)

/**
    Create a Mutex lock object.
    @description This call creates a Mutex lock object that can be used in #mprLock, #mprTryLock and #mprUnlock calls.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC MprMutex *mprCreateLock(void);

/**
    Initialize a statically allocated Mutex lock object.
    @description This call initialized a Mutex lock object without allocation. The object can then be used used
        in #mprLock, #mprTryLock and #mprUnlock calls.
    @param mutex Reference to an MprMutex structure to initialize
    @returns A reference to the supplied mutex. Returns null on errors.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC MprMutex *mprInitLock(MprMutex *mutex);

/**
    Attempt to lock access.
    @description This call attempts to assert a lock on the given \a lock mutex so that other threads calling
        mprLock or mprTryLock will block until the current thread calls mprUnlock.
    @returns Returns zero if the successful in locking the mutex. Returns a negative MPR error code if unsuccessful.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC bool mprTryLock(MprMutex *lock);

/**
    Create a spin lock lock object.
    @description This call creates a spinlock object that can be used in #mprSpinLock, and #mprSpinUnlock calls. Spin locks
        using MprSpin are much faster than MprMutex based locks on some systems.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC MprSpin *mprCreateSpinLock(void);

/**
    Initialize a statically allocated spinlock object.
    @description This call initialized a spinlock lock object without allocation. The object can then be used used
        in #mprSpinLock and #mprSpinUnlock calls.
    @param lock Reference to a static #MprSpin  object.
    @returns A reference to the MprSpin object. Returns null on errors.
    @ingroup MprSync
 */
PUBLIC MprSpin *mprInitSpinLock(MprSpin *lock);

/**
    Attempt to lock access on a spin lock
    @description This call attempts to assert a lock on the given \a spin lock so that other threads calling
        mprSpinLock or mprTrySpinLock will block until the current thread calls mprSpinUnlock.
    @returns Returns zero if the successful in locking the spinlock. Returns a negative MPR error code if unsuccessful.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC bool mprTrySpinLock(MprSpin *lock);

/*
    For maximum performance, use the spin lock/unlock routines macros
 */
#if !ME_DEBUG
#define ME_USE_LOCK_MACROS 1
#endif
#if ME_USE_LOCK_MACROS && !DOXYGEN
    /*
        Spin lock macros
     */
    #if ME_UNIX_LIKE && ME_COMPILER_HAS_SPINLOCK
        #define mprSpinLock(lock)   if (lock) pthread_spin_lock(&((lock)->cs))
        #define mprSpinUnlock(lock) if (lock) pthread_spin_unlock(&((lock)->cs))
    #elif ME_UNIX_LIKE
        #define mprSpinLock(lock)   if (lock) pthread_mutex_lock(&((lock)->cs))
        #define mprSpinUnlock(lock) if (lock) pthread_mutex_unlock(&((lock)->cs))
    #elif ME_WIN_LIKE
        #define mprSpinLock(lock)   if (lock && (!((MprSpin*)(lock))->freed)) EnterCriticalSection(&((lock)->cs))
        #define mprSpinUnlock(lock) if (lock) LeaveCriticalSection(&((lock)->cs))
    #elif VXWORKS
        #define mprSpinLock(lock)   if (lock) semTake((lock)->cs, WAIT_FOREVER)
        #define mprSpinUnlock(lock) if (lock) semGive((lock)->cs)
    #endif

    /*
        Lock macros
     */
    #if ME_UNIX_LIKE
        #define mprLock(lock)       if (lock) pthread_mutex_lock(&((lock)->cs))
        #define mprUnlock(lock)     if (lock) pthread_mutex_unlock(&((lock)->cs))
    #elif ME_WIN_LIKE
        #define mprLock(lock)       if (lock && !(((MprSpin*)(lock))->freed)) EnterCriticalSection(&((lock)->cs))
        #define mprUnlock(lock)     if (lock) LeaveCriticalSection(&((lock)->cs))
    #elif VXWORKS
        #define mprLock(lock)       if (lock) semTake((lock)->cs, WAIT_FOREVER)
        #define mprUnlock(lock)     if (lock) semGive((lock)->cs)
    #endif
#else

    /**
        Lock access.
        @description This call asserts a lock on the given \a lock mutex so that other threads calling mprLock will
            block until the current thread calls mprUnlock.
        @ingroup MprSync
        @stability Stable.
     */
    PUBLIC void mprLock(MprMutex *lock);

    /**
        Unlock a mutex.
        @description This call unlocks a mutex previously locked via mprLock or mprTryLock.
        @ingroup MprSync
        @stability Stable.
     */
    PUBLIC void mprUnlock(MprMutex *lock);

    /**
        Lock a spinlock.
        @description This call asserts a lock on the given \a spinlock so that other threads calling mprSpinLock will
            block until the curren thread calls mprSpinUnlock.
        @ingroup MprSync
        @stability Stable.
     */
    PUBLIC void mprSpinLock(MprSpin *lock);

    /**
        Unlock a spinlock.
        @description This call unlocks a spinlock previously locked via mprSpinLock or mprTrySpinLock.
        @ingroup MprSync
        @stability Stable.
     */
    PUBLIC void mprSpinUnlock(MprSpin *lock);
#endif

/**
    Globally lock the application.
    @description This call asserts the application global lock so that other threads calling mprGlobalLock will
        block until the current thread calls mprGlobalUnlock.  WARNING: Use this API very sparingly.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC void mprGlobalLock(void);

/**
    Unlock the global mutex.
    @description This call unlocks the global mutex previously locked via mprGlobalLock.
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC void mprGlobalUnlock(void);

/*
    Lock free primitives
 */

/**
    Open and initialize the atomic subystem
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC void mprAtomicOpen(void);

/**
    Apply a full (read+write) memory barrier
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC void mprAtomicBarrier(void);

/**
    Atomic list insertion. Inserts "item" at the "head" of the list. The "link" field is the next field in item.
    This is a lock-free function
    @param head list head
    @param link Reference to the list head link field
    @param item Item to insert
    @ingroup MprSync
    @stability Stable
 */
PUBLIC void mprAtomicListInsert(void **head, void **link, void *item);

/**
    Atomic Compare and Swap. This is a lock free function.
    @param target Address of the target word to swap
    @param expected Expected value of the target
    @param value New value to store at the target
    @return TRUE if the swap was successful
    @ingroup MprSync
    @stability Stable
 */
PUBLIC int mprAtomicCas(void * volatile * target, void *expected, cvoid *value);

/**
    Atomic Add. This is a lock free function.
    @param target Address of the target word to add to.
    @param value Value to add to the target
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC void mprAtomicAdd(volatile int *target, int value);

/**
    Atomic 64 bit Add. This is a lock free function.
    @param target Address of the target word to add to.
    @param value Value to add to the target
    @ingroup MprSync
    @stability Stable.
 */
PUBLIC void mprAtomicAdd64(volatile int64 *target, int64 value);

/********************************* Memory Allocator ***************************/
/*
    Allocator debug and stats selection
    To set via configure:
         configure --set mpr.alloc.check=true
         configure --set mpr.alloc.cache=NNN
         configure --set mpr.alloc.quota=NNN
 */
#if ME_MPR_ALLOC_CHECK
    #ifndef ME_MPR_ALLOC_DEBUG
        #define ME_MPR_ALLOC_DEBUG     1                   /**< Fill blocks, verifies block integrity, block names */
    #endif
    #ifndef ME_MPR_ALLOC_STATS
        #define ME_MPR_ALLOC_STATS     1                   /**< Include memory statistics */
    #endif
    #ifndef ME_MPR_ALLOC_STACK
        #define ME_MPR_ALLOC_STACK     1                   /**< Monitor stack usage */
    #endif
#else
    #ifndef ME_MPR_ALLOC_DEBUG
        #define ME_MPR_ALLOC_DEBUG     0
    #endif
    #ifndef ME_MPR_ALLOC_STATS
        #define ME_MPR_ALLOC_STATS     0
    #endif
    #ifndef ME_MPR_ALLOC_STACK
        #define ME_MPR_ALLOC_STACK     0
    #endif
#endif

/*
    Allocator Tunables
 */
#ifndef ME_MPR_ALLOC_CACHE
    /*
        Try to cache at least this amount in the heap free queues
     */
    #if ME_TUNE_SIZE
        #define ME_MPR_ALLOC_CACHE 0
    #elif ME_TUNE_SPEED
        #define ME_MPR_ALLOC_CACHE (1 * 1024 * 1024)   /* 1MB */
    #else
        #define ME_MPR_ALLOC_CACHE ME_MPR_ALLOC_REGION_SIZE
    #endif
#endif
#ifndef ME_MPR_ALLOC_LEVEL
    #define ME_MPR_ALLOC_LEVEL     7                    /* Emit mark/sweek elapsed time at this level */
#endif
#if ME_COMPILER_HAS_MMU
    #define ME_MPR_ALLOC_VIRTUAL   1                    /* Use virtual memory allocations */
#else
    #define ME_MPR_ALLOC_VIRTUAL   0                    /* Use malloc() for region allocations */
#endif
#ifndef ME_MPR_ALLOC_QUOTA
    #if ME_TUNE_SIZE
        #define ME_MPR_ALLOC_QUOTA  (200 * 1024)        /* Total allocations before a GC is worthwhile */
    #else
        #define ME_MPR_ALLOC_QUOTA  (512 * 1024)
    #endif
#endif
#ifndef ME_MPR_ALLOC_REGION_SIZE
    #define ME_MPR_ALLOC_REGION_SIZE (256 * 1024)       /* Memory region allocation chunk size */
#endif

#ifndef ME_MPR_ALLOC_ALIGN_SHIFT
    /*
        Allocated block alignment expressed as a bit shift. The default alignment is set so that allocated memory can be used
        for doubles. NOTE: SSE and AltiVec instuctions may require 16 byte alignment.
     */
    #if !ME_64 && !(ME_CPU_ARCH == ME_CPU_MIPS)
        #define ME_MPR_ALLOC_ALIGN_SHIFT 3              /* 8 byte alignment */
    #else
        #define ME_MPR_ALLOC_ALIGN_SHIFT 3
    #endif
#endif
#define ME_MPR_ALLOC_ALIGN (1 << ME_MPR_ALLOC_ALIGN_SHIFT)

/*
    The allocator (by default) is limited to individual allocations of 4GB (32 bits). This enables memory blocks to
    be optimally aligned with minimal overhead. Define ME_MPR_ALLOC_BIG on 64-bit systems to enable allocating blocks
    greater than 4GB.
 */
#if ME_MPR_ALLOC_BIG && ME_64
    typedef uint64 MprMemSize;
#else
    typedef uint MprMemSize;
#endif
#define MPR_ALLOC_MAX ((MprMemSize) - ME_MPR_ALLOC_ALIGN)

/**
    Memory Allocation Service.
    @description The MPR provides an application specific memory allocator to use instead of malloc. This allocator is
    tailored to the needs of embedded applications and is faster than most general purpose malloc allocators. It is
    deterministic and allocates and frees in constant time O(1). It exhibits very low fragmentation and accurate
    coalescing.
    \n\n
    The allocator uses a garbage collector for freeing unused memory. The collector is a cooperative, non-compacting,
    parallel collector.  The allocator is optimized for frequent allocations of small blocks (< 4K) and uses a scheme
    of free queues for fast allocation. Allocations are aligned as specified by ME_MPR_ALLOC_ALIGN_SHIFT. This is typically
    16 byte aligned for 64-bit systems and 8 byte aligned for 32-bit systems. The allocator will return unused memory
    back to the O/S to minimize application memory footprint.
    \n\n
    The allocator handles memory allocation errors globally. The application may configure a memory limit so that
    memory depletion can be proactively detected and handled before memory allocations actually fail.
    \n\n
    A memory block that is being used must be marked as active to prevent the garbage collector from reclaiming it.
    To mark a block as active, #mprMark must be called during each garbage collection cycle. When allocating
    non-temporal memory blocks, a manager callback can be specified via #mprAllocObj. This manager routine will be
    called by the collector so that dependent memory blocks can be marked as active.
    \n\n
    The collector performs the marking phase by invoking the manager routines for a set of root blocks. A block can be
    added to the set of roots by calling #mprAddRoot. Each root's manager routine will mark other blocks which will cause
    their manager routines to run and so on, until all active blocks have been marked. Non-marked blocks can then safely
    be reclaimed as garbage. A block may alternatively be permanently marked as active by calling #mprHold.
    \n\n
    The mark phase begins when all threads explicitly "yield" to the garbage collector. This cooperative approach ensures
    that user threads will not inadvertendly loose allocated blocks to the collector. Once all active blocks are marked,
    user threads are resumed and the garbage sweeper frees unused blocks in parallel with user threads.

    @stability Internal
    @defgroup MprMem MprMem
    @see MprFreeMem MprHeap MprManager MprMemNotifier MprRegion mprAddRoot mprAlloc mprAllocMem mprAllocObj
        mprAllocZeroed mprCreateMemService mprDestroyMemService mprEnableGC mprGetBlockSize mprGetMem
        mprGetMemStats mprGetMpr mprGetPageSize mprHasMemError mprHold mprIsPathContained mprIsValid mprMark
        mprMemcmp mprMemcpy mprMemdup mprPrintMem mprRealloc mprRelease mprRemoveRoot mprGC mprResetMemError
        mprRevive mprSetAllocLimits mprSetManager mprSetMemError mprSetMemLimits mprSetMemNotifier mprSetMemPolicy
        mprSetName mprVerifyMem mprVirtAlloc mprVirtFree
 */
typedef struct MprMem {
    MprMemSize  size;                   /**< Size of the block in bytes. Not the amount requested by the user which
                                             may be smaller. This is a 32-bit quantity on all systems unless
                                             ME_MPR_ALLOC_BIG is defined and then it will be 64 bits. */
    uchar       qindex;                 /**< Freeq index. Always less than 512 queues. */
    uchar       eternal;                /**< Immune from GC. Implemented as a byte to be atomic */
    uchar       free: 1;                /**< Block not in use */
    uchar       first: 1;               /**< Block is first block in region */
    uchar       hasManager: 1;          /**< Has manager function. Set at block init. */
    uchar       mark: 1;                /**< GC mark indicator. Toggled for each GC pass by mark() when thread yielded. */
    uchar       fullRegion: 1;          /**< Block is an entire region - never on free queues . */

#if ME_MPR_ALLOC_DEBUG
    /* This increases the size of MprMem from 8 bytes to 16 bytes on 32-bit systems and 24 bytes on 64 bit systems */
    cchar       *name;                  /**< Debug name */
    ushort      magic;                  /**< Unique signature */
    ushort      seqno;                  /**< Allocation sequence number */
#if ME_64
    uchar       filler[4];
#endif
#else
    uchar       filler[1];
#endif
} MprMem;

/**
    Block structure when on a free list. This overlays MprMem and replaces sibling and children with forw/back
    The implies a minimum memory block size of 16 bytes.
    @ingroup MprMem
    @stability Internal.
 */
typedef struct MprFreeMem {
    MprMem              blk;
    struct MprFreeMem   *prev;          /**< Previous free block */
    struct MprFreeMem   *next;          /**< Next free block */
} MprFreeMem;


/**
    Free queue head structure. These must share the same layout as MprFreeMem for the prev/next pointers.
 */
typedef struct MprFreeQueue {
    MprMem              blk;            /**< Unused in queue head */
    struct MprFreeMem   *prev;          /**< Previous free block */
    struct MprFreeMem   *next;          /**< Next free block */
    MprSpin             lock;           /**< Queue lock-free lock */
    uint                count;          /**< Number of blocks on the queue */
    MprMemSize          minSize;        /**< Minimum size of blocks in queue. This is the user block size sans
                                             MprMem header. */
} MprFreeQueue;

#define MPR_ALLOC_ALIGN(x)          (((x) + ME_MPR_ALLOC_ALIGN - 1) & ~(ME_MPR_ALLOC_ALIGN - 1))
#define MPR_ALLOC_MIN_BLOCK         sizeof(MprFreeMem)
#define MPR_ALLOC_MAX_BLOCK         (ME_MPR_ALLOC_REGION_SIZE - sizeof(MprRegion))
#define MPR_ALLOC_MIN_SPLIT         (32 + sizeof(MprMem))
#define MPR_ALLOC_MAGIC             0xe813

#define MPR_PAGE_ALIGN(x, psize)    ((((ssize) (x)) + ((ssize) (psize)) - 1) & ~(((ssize) (psize)) - 1))
#define MPR_PAGE_ALIGNED(x, psize)  ((((ssize) (x)) % ((ssize) (psize))) == 0)

/*
    The allocator has a set of free queues to hold blocks of a given size range. Higher queues progressively address
    a larger range of block sizes. This mapping is achived by taking the most significant QBIT bits of the requested
    block size and then discarding the top bit (MSB). All combinations of the REST bits are mapped to the same queue.

    +-------------------------------+
    |        QBits      |    REST   |
    +-------------------------------+
    | 0 | 1 | X | X | X | ......... |
    +-------------------------------+
    | 1 | X | X | X | ............. |
    +-------------------------------+

    A bitmap records for each queue whether it has any free blocks in the queue.
    Note: qindex 2 is the first queue used because the minimum block size is sizeof(MprFreeMem)
 */
#define MPR_ALLOC_QBITS_SHIFT       2
#define MPR_ALLOC_NUM_QBITS         (1 << MPR_ALLOC_QBITS_SHIFT)

/*
    Should set region shift to log(ME_MPR_ALLOC_REGION_SIZE)
    We don't expect users to tinker with these
 */
#if ME_MPR_ALLOC_REGION_SIZE == (128 * 1024)
    #define ME_MPR_ALLOC_REGION_SHIFT 18
#elif ME_MPR_ALLOC_REGION_SIZE == (256 * 1024)
    #define ME_MPR_ALLOC_REGION_SHIFT 19
#elif ME_MPR_ALLOC_REGION_SIZE == (512 * 1024)
    #define ME_MPR_ALLOC_REGION_SHIFT 20
#else
    #define ME_MPR_ALLOC_REGION_SHIFT 24
#endif

#define MPR_ALLOC_NUM_QUEUES        ((ME_MPR_ALLOC_REGION_SHIFT - ME_MPR_ALLOC_ALIGN_SHIFT - MPR_ALLOC_QBITS_SHIFT) * \
                                        MPR_ALLOC_NUM_QBITS)
#define MPR_ALLOC_BITMAP_BITS       BITS(size_t)
#define MPR_ALLOC_NUM_BITMAPS       ((MPR_ALLOC_NUM_QUEUES + MPR_ALLOC_BITMAP_BITS - 1) / MPR_ALLOC_BITMAP_BITS)

/*
    Pointer to MprMem and vice-versa
 */
#define MPR_GET_PTR(bp)             ((void*) (((char*) (bp)) + sizeof(MprMem)))
#define MPR_GET_MEM(ptr)            ((MprMem*) (((char*) (ptr)) - sizeof(MprMem)))
#define MPR_GET_USIZE(mp)           ((size_t) (mp->size - sizeof(MprMem) - (mp->hasManager * sizeof(void*))))

/*
    Manager callback is stored in the padding region at the end of the user memory in the block.
 */
#define MPR_MANAGER_SIZE            1
#define MPR_MANAGER_OFFSET          1
#define MPR_MEM_PAD_PTR(mp, offset) ((void*) (((char*) mp) + mp->size - ((offset) * sizeof(void*))))
#define GET_MANAGER(mp)             ((MprManager) (*(void**) ((MPR_MEM_PAD_PTR(mp, MPR_MANAGER_OFFSET)))))
#define SET_MANAGER(mp, fn)         do { \
                                        *((MprManager*) MPR_MEM_PAD_PTR(mp, MPR_MANAGER_OFFSET)) = fn ; \
                                        mp->hasManager = 1; \
                                    } while (0);
/*
    Manager callback flags
 */
#define MPR_MANAGE_FREE             0x1         /**< Block being freed. Free dependant resources */
#define MPR_MANAGE_MARK             0x2         /**< Block being marked by GC. Mark dependant resources */

/*
    VirtAloc flags
 */
#if ME_WIN_LIKE || VXWORKS
    #define MPR_MAP_READ            0x1
    #define MPR_MAP_WRITE           0x2
    #define MPR_MAP_EXECUTE         0x4
#else
    #define MPR_MAP_READ            PROT_READ
    #define MPR_MAP_WRITE           PROT_WRITE
    #define MPR_MAP_EXECUTE         PROT_EXEC
#endif

#if ME_MPR_ALLOC_DEBUG
    #define MPR_CHECK_BLOCK(bp)     mprCheckBlock(bp)
    #define MPR_VERIFY_MEM()        if (MPR->heap->verify) { mprVerifyMem(); } else {}
#else
    #define MPR_CHECK_BLOCK(bp)
    #define MPR_VERIFY_MEM()
#endif

/*
    Memory depletion policy (mprSetAllocPolicy)
 */
#define MPR_ALLOC_POLICY_NOTHING    0       /**< Do nothing */
#define MPR_ALLOC_POLICY_PRUNE      1       /**< Prune all non-essential memory and continue */
#define MPR_ALLOC_POLICY_RESTART    2       /**< Gracefully restart the app if memory maxHeap level is exceeded */
#define MPR_ALLOC_POLICY_EXIT       3       /**< Exit the app if maxHeap exceeded */

/*
    MprMemNotifier cause argument
 */
#define MPR_MEM_WARNING             0x1         /**< Memory use exceeds warnHeap level limit */
#define MPR_MEM_LIMIT               0x2         /**< Memory use exceeds memory limit - invoking policy */
#define MPR_MEM_FAIL                0x4         /**< Memory allocation failed - immediate exit */
#define MPR_MEM_TOO_BIG             0x8         /**< Memory allocation request is too big - immediate exit */

/**
    Memory allocation error callback. Notifiers are called if a low memory condition exists.
    @param cause Set to the cause of the memory error. Set to #MPR_MEM_WARNING if the allocation will exceed the warnHeap
        limit. Set to #MPR_MEM_LIMIT if it would exceed the maxHeap memory limit. Set to #MPR_MEM_FAIL if the
        allocation failed.
        Set to #MPR_MEM_TOO_BIG if the allocation block size is too large.
        Allocations will be rejected for MPR_MEM_FAIL and MPR_MEM_TOO_BIG, otherwise the allocations will proceed and the
        memory notifier will be invoked.
    @param policy Memory depletion policy. Set to one of #MPR_ALLOC_POLICY_NOTHING, #MPR_ALLOC_POLICY_PRUNE,
        #MPR_ALLOC_POLICY_RESTART or #MPR_ALLOC_POLICY_EXIT.
    @param size Size of the allocation that triggered the low memory condition.
    @param total Total memory currently in use
    @ingroup MprMem
    @stability Stable.
 */
typedef void (*MprMemNotifier)(int cause, int policy, size_t size, size_t total);

/**
    Mpr memory block manager prototype
    @param ptr Any memory context allocated by the MPR.
    @ingroup MprMem
    @stability Stable.
 */
typedef void (*MprManager)(void *ptr, int flags);

#if ME_MPR_ALLOC_DEBUG
/*
    The location stats table tracks the source code location responsible for each allocation
    Very costly. Don't use except for debug.
 */
#define MPR_TRACK_HASH        2053          /* Size of location name hash */
#define MPR_TRACK_NAMES       8             /* Length of collision chain */

typedef struct MprLocationStats {
    size_t  total;                          /* Total allocations for this location */
    int     count;                          /* Count of allocations for this location */
    cchar   *names[MPR_TRACK_NAMES];        /* Manager names */
} MprLocationStats;
#endif


/**
    Memory allocator statistics
    @ingroup MprMem
    @stability Internal.
  */
typedef struct MprMemStats {
    int             inMemException;         /**< Recursive protect */
    uint            cpuCores;               /**< Number of CPU cores */
    uint            pageSize;               /**< System page size */
    uint            heapRegions;            /**< Heap region count */
    uint            sweeps;                 /**< Number of GC sweeps */
    uint64          cpuUsage;               /**< Process CPU usage in ticks */
    uint64          cacheHeap;              /**< Heap cache. Try to keep at least this amount in the free queues  */
    uint64          bytesAllocated;         /**< Bytes currently allocated. Includes active and free. */
    uint64          bytesAllocatedPeak;     /**< Max ever bytes allocated */
    uint64          bytesFree;              /**< Bytes currently free and retained in the heap queues */
    uint64          errors;                 /**< Allocation errors */
    uint64          lowHeap;                /**< Low memory level at which to initiate a collection */
    uint64          maxHeap;                /**< Max memory that can be allocated */
    uint64          ram;                    /**< System RAM size in bytes */
    uint64          rss;                    /**< OS calculated memory resident set size in bytes */
    uint64          user;                   /**< System user RAM size in bytes (excludes kernel) */
    uint64          warnHeap;               /**< Warn if heap size exceeds this level */
    uint64          swept;                  /**< Number of blocks swept */
    uint64          sweptBytes;             /**< Number of bytes swept */
#if ME_MPR_ALLOC_STATS
    /*
        Extended memory stats
     */
    uint64          allocs;                 /**< Count of times a block was split Calls to allocate memory from the O/S */
    uint64          cached;                 /**< Count of blocks that are cached rather then joined with adjacent blocks */
    uint64          compacted;              /**< Count of blocks that are compacted during compacting sweeps */
    uint64          collections;            /**< Number of GC collections */
    uint64          freed;                  /**< Bytes freed in last sweep */
    uint64          joins;                  /**< Count of times a block was joined (coalesced) with its neighbours */
    uint64          markVisited;            /**< Number of blocks examined for marking */
    uint64          marked;                 /**< Number of blocks marked */
    uint64          race;                   /**< Another thread raced for a block and won */
    uint64          requests;               /**< Count of memory requests */
    uint64          reuse;                  /**< Count of times a block was reused from a free queue */
    uint64          retries;                /**< Queue retries */
    uint64          qrace;                  /**< Count of times a queue was empty - racing with another thread */
    uint64          splits;                 /**< Count of times a block was split */
    uint64          sweepVisited;           /**< Number of blocks examined for sweeping */
    uint64          trys;                   /**< Attempts to acquire a freeq */
    uint64          tryFails;               /** Acquire a freeq fail count */
    uint64          unpins;                 /**< Count of times a block was unpinned and released back to the O/S */
#endif
#if ME_MPR_ALLOC_DEBUG
    MprLocationStats locations[MPR_TRACK_HASH]; /* Per location allocation stats */
#endif
} MprMemStats;


/**
    Memmory regions allocated from the O/S
    @ingroup MprMem
    @stability Internal.
 */
typedef struct MprRegion {
    struct MprRegion *next;                 /**< Next region */
    MprMem           *start;                /**< Start of region data */
    MprMem           *end;                  /**< End of region data */
    size_t           size;                  /**< Size of region including region header */
    int              freeable;              /**< Set to true when completely unused */
} MprRegion;


/**
    Memory allocator heap
    @ingroup MprMem
    @stability Internal.
 */
typedef struct MprHeap {
    MprFreeQueue     freeq[MPR_ALLOC_NUM_QUEUES]; /**< Heap free queues */
    size_t           bitmap[MPR_ALLOC_NUM_BITMAPS]; /* Freeq bit map. Must be size_t for cas() */
    struct MprList   *roots;                /**< List of GC root objects */
    MprMemStats      stats;                 /**< Memory allocation statistics */
    MprMemNotifier   notifier;              /**< Memory allocation failure callback */
    MprCond          *gcCond;               /**< GC sleep cond var */
    MprRegion        *regions;              /**< List of memory regions */
    struct MprThread *sweeper;              /**< GC sweeper thread */
    int              allocPolicy;           /**< Memory allocation depletion policy */
    int              regionSize;            /**< Memory allocation region size */
    int              compact;               /**< Next GC sweep should do a full compact */
    int              collecting;            /**< Manual GC is running */
    int              freedBlocks;           /**< True if the last sweep freed blocks */
    int              flags;                 /**< GC operational control flags */
    int              from;                  /**< Eligible mprCollectGarbage flags */
    int              gcEnabled;             /**< GC is enabled */
    int              gcRequested;           /**< GC has been requested */
    int              hasError;              /**< Memory allocation error */
    int              mark;                  /**< Mark version */
    int              marking;               /**< Actually marking objects now */
    int              mustYield;             /**< Threads must yield for GC which is due */
    int              nextSeqno;             /**< Next sequence number */
    int              pageSize;              /**< System page size */
    int              printStats;            /**< Print diagnostic heap statistics */
    uint64           priorFree;             /**< Last sweep free memory */
    uint64           priorWorkDone;         /**< Prior workDone before last sweep */
    int              scribble;              /**< Scribble over freed memory (slow) */
    int              sweeping;              /**< Actually sweeping objects now */
    int              track;                 /**< Track memory allocations (requires ME_MPR_ALLOC_DEBUG) */
    int              verify;                /**< Verify memory contents (very slow) */
    uint64           workDone;              /**< Count of allocations weighted by block size */
    uint64           workQuota;             /**< Quota of work done before idle GC worthwhile */
} MprHeap;

/**
    Create and initialize the Memory service
    @description Called internally by the MPR. Should not be called by users.
    @param manager Memory manager to manage the Mpr object
    @param flags Memory initialization control flags
    @return The Mpr control structure
    @ingroup MprMem
    @stability Internal.
 */
PUBLIC struct Mpr *mprCreateMemService(MprManager manager, int flags);

/*
    Flags for mprAllocMem
 */
#define MPR_ALLOC_MANAGER           0x1         /**< Reserve room for a manager */
#define MPR_ALLOC_ZERO              0x2         /**< Zero memory */
#define MPR_ALLOC_HOLD              0x4         /**< Allocate and hold */
#define MPR_ALLOC_PAD_MASK          0x1         /**< Flags that impact padding */

/**
    Allocate a block of memory.
    @description This is the lowest level of memory allocation routine. Memory is freed via the garbage collector.
    To protect an active memory block memory block from being reclaimed, it must have a reference to it. Memory blocks
    can specify a manager routine via #mprAllocObj. The manager is is invoked by the garbage collector to "mark"
    dependant active blocks. Marked blocks will not be reclaimed by the garbage collector.
    This function can be called by foreign (non Mpr) threads provided you use the MPR_ALLOC_HOLD flag so that the memory
    will be preserved until you call mprRelease on the memory block. This is important as without the MPR_ALLOC_HOLD flag,
    the garbage collector could run immediately after calling mprAlloc and collect the memory. When used in an Mpr thread,
    the garbage collector cannot run unless you call mprYield and so the memory is safe from immediate collection.
    @param size Size of the memory block to allocate.
    @param flags Allocation flags. Supported flags include: MPR_ALLOC_MANAGER to reserve room for a manager callback and
        MPR_ALLOC_ZERO to zero allocated memory. Use MPR_ALLOC_HOLD to return memory immune from GC. Must use this flag if
        calling from a foreign thread. Use #mprRelease to release back to the system.
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion handler
        specified via mprCreate will be called to allow global recovery.
    @remarks Do not mix calls to malloc and mprAlloc.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void *mprAllocMem(size_t size, int flags);

/**
    Return the process CPU usage.
    @returns The total number of ticks of cpu usage since process tart
    @ingroup MprMem
    @stability Stable
 */
PUBLIC uint64 mprGetCPU(void);

/**
    Return the current allocation memory statistics block
    @returns a reference to the allocation memory statistics. Do not modify its contents.
    @ingroup MprMem
    @stability Internal.
 */
PUBLIC MprMemStats *mprGetMemStats(void);

/**
    Return the amount of memory currently used by the application. On Unix, this returns the total application memory
    size including code, stack, data and heap. On Windows, VxWorks and other operatings systems, it returns the
    amount of allocated heap memory.
    @returns the amount of memory used by the application in bytes.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC size_t mprGetMem(void);

/**
    Get the current O/S virtual page size
    @returns the page size in bytes
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC int mprGetPageSize(void);

/**
    Get the allocated size of a memory block
    @param ptr Any memory allocated by mprAlloc
    @returns the block size in bytes
    @ingroup MprMem
    @stability Internal.
 */
PUBLIC size_t mprGetBlockSize(cvoid *ptr);

/**
    Determine if the MPR has encountered memory allocation errors.
    @description Returns true if the MPR has had a memory allocation error. Allocation errors occur if any
        memory allocation would cause the application to exceed the configured warnHeap limit, or if any O/S memory
        allocation request fails.
    @return TRUE if a memory allocation error has occurred. Otherwise returns FALSE.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC bool mprHasMemError(void);

/**
    Test is a pointer is a valid memory context. This is used to test if a block has been dynamically allocated.
    @param ptr Any memory context allocated by mprAlloc or mprCreate.
    @ingroup MprMem
    @stability Internal.
 */
PUBLIC int mprIsValid(cvoid *ptr);

/**
    Compare two byte strings.
    @description Safely compare two byte strings. This is a safe replacement for memcmp.
    @param b1 Pointer to the first byte string.
    @param b1Len Length of the first byte string.
    @param b2 Pointer to the second byte string.
    @param b2Len Length of the second byte string.
    @return Returns zero if the byte strings are identical. Otherwise returns -1 if the first string is less than the
        second. Returns 1 if the first is greater than the first.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC int mprMemcmp(cvoid *b1, size_t b1Len, cvoid *b2, size_t b2Len);

/**
    Safe copy for a block of data.
    @description Safely copy a block of data into an existing memory block. The call ensures the destination
        block is not overflowed and returns the size of the block actually copied. This is similar to memcpy, but
        is a safer alternative.
    @param dest Pointer to the destination block.
    @param destMax Maximum size of the destination block.
    @param src Block to copy
    @param nbytes Size of the source block
    @return Returns the number of characters in the allocated block.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC size_t mprMemcpy(void *dest, size_t destMax, cvoid *src, size_t nbytes);

/**
    Duplicate a block of memory.
    @description Copy a block of memory into a newly allocated block.
    @param ptr Pointer to the block to duplicate.
    @param size Size of the block to copy.
    @return Returns an allocated block.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void *mprMemdup(cvoid *ptr, size_t size);

#define MPR_MEM_DETAIL      0x1     /* Print a detailed report */

/**
    Print a memory usage report to stdout
    @param msg Prefix message to the report
    @param flags Set to MPR_MEM_DETAIL for a detailed memory report
    @ingroup MprMem
    @stability Internal.
 */
PUBLIC void mprPrintMem(cchar *msg, int flags);

/**
    Reallocate a block
    @description Reallocates a block increasing its size. If the specified size is less than the current block size,
        the call will ignore the request and simply return the existing block. The new memory portion is not zeroed.
    @param ptr Memory to reallocate. If NULL, call malloc.
    @param size New size of the required memory block.
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion handler
        specified via mprCreate will be called to allow global recovery.
    @remarks Do not mix calls to realloc and mprRealloc.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void *mprRealloc(void *ptr, size_t size);

/**
    Reset the memory allocation error flag
    @description Reset the alloc error flag triggered.
    @ingroup MprMem
    @stability Internal.
 */
PUBLIC void mprResetMemError(void);

/*
    Revive a memory block scheduled for collection. This should only ever be called in the manager routine for a block
    when the manage flags parameter is set to MPR_MANAGE_FREE. Reviving a block aborts its collection.
    @param ptr Reference to an allocated memory block.
    @internal
    @stability Internal.
 */
PUBLIC void mprRevive(cvoid* ptr);

/**
    Define a memory notifier
    @description A notifier callback will be invoked for memory allocation errors for the given memory context.
    @param cback Notifier callback function
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void mprSetMemNotifier(MprMemNotifier cback);

/**
    Set an memory allocation error condition on a memory context. This will set an allocation error condition on the
    given context and all its parents. This way, you can test the ultimate parent and detect if any memory allocation
    errors have occurred.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void mprSetMemError(void);

/**
    Configure the application memory limits
    @description Configure memory limits to constrain memory usage by the application. The memory allocation subsystem
        will check these limits before granting memory allocation requrests. The warnHeap is a soft limit that if exceeded
        will invoke the memory allocation callback, but will still honor the request. The maximum limit is a hard limit.
        The MPR will prevent allocations which exceed this maximum. The memory callback handler is defined via
        the #mprCreate call.
    @param warnHeap Soft memory limit. If exceeded, the request will be granted, but the memory handler will be invoked.
        to issue a warning and potentially take remedial acation.  If -1, then do not update the warnHeap.
    @param maximum Hard memory limit. If exceeded, the request will not be granted, and the memory handler will be invoked.
        If -1, then do not update the maximum.
    @param cache Heap cache. Try to keep at least this amount of memory in the heap free queues
        If -1, then do not update the cache.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void mprSetMemLimits(ssize warnHeap, ssize maximum, ssize cache);

/**
    Set the memory allocation policy for when allocations fail.
    @param policy Set to MPR_ALLOC_POLICY_EXIT for the application to immediately exit on memory allocation errors.
        Set to MPR_ALLOC_POLICY_RESTART to restart the appplication on memory allocation errors.
    @ingroup MprMem
    @stability Stable.
*/
PUBLIC void mprSetMemPolicy(int policy);

/**
    Update the manager for a block of memory.
    @description This call updates the manager for a block of memory allocated via mprAllocWithManager.
    @param ptr Memory to free. If NULL, take no action.
    @param manager Manager function to invoke when the memory is released.
    @return Returns the original object
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void *mprSetManager(void *ptr, MprManager manager);

/**
    Memory virtual memory into the applications address space.
    @param size of virtual memory to map. This size will be rounded up to the nearest page boundary.
    @param mode Mask set to MPR_MAP_READ | MPR_MAP_WRITE
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void *mprVirtAlloc(size_t size, int mode);

/**
    Free (unpin) a mapped section of virtual memory
    @param ptr Virtual address to free. Should be page aligned
    @param size Size of memory to free in bytes
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void mprVirtFree(void *ptr, size_t size);

/**
    Allocate a "permanent" block of memory that is not subject GC.
    @description This allocates a block of memory using the MPR allocator. It then calls mprHold on the block.
    to prevent GC from freeing the block.
    @param size Size of the memory block to allocate.
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion handler
        specified via mprCreate will be called to allow global recovery.
    @remarks Do not mix calls to palloc and malloc.
    @ingroup MprMem
    @stability Stable
 */
PUBLIC void *palloc(size_t size);

/**
    Free a "permanent" block of memory allocated via "palloc".
    @description This releases a block of memory allocated via "palloc" to be collected by the garbage collector.
    @param ptr Pointer to the block
    @remarks Do not mix calls to pfree and free.
    @ingroup MprMem
    @stability Stable
 */
PUBLIC void pfree(void *ptr);

/**
    Reallocate a "permanent" block of memory allocated via "palloc".
    This function should not be used by foreign (non Mpr) threads.
    @description This increases the size of a block of memory allocated via "palloc".
    @param ptr Pointer to the block
    @param size New block size
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion handler
        specified via mprCreate will be called to allow global recovery.
    @remarks Do not mix calls to prealloc and malloc.
    @ingroup MprMem
    @stability Stable
 */
PUBLIC void *prealloc(void *ptr, size_t size);

/**
    Return the size of the block. This may be larger than what was originally requested.
    This function should not be used by foreign (non Mpr) threads.
    @param ptr Pointer to the block
    @return Size of the allocated block.
    @ingroup MprMem
    @stability Stable
 */
PUBLIC size_t psize(void *ptr);

/*
    Macros. When building documentation (DOXYGEN), define pretend function defintions for the documentation.
 */
/*
    In debug mode, all memory blocks can have a debug name
 */
#if ME_MPR_ALLOC_DEBUG
    PUBLIC void *mprSetName(void *ptr, cchar *name);
    PUBLIC void *mprCopyName(void *dest, void *src);
    #define mprGetName(ptr) (MPR_GET_MEM(ptr)->name)
    PUBLIC void *mprSetAllocName(void *ptr, cchar *name);
#else
    #define mprCopyName(dest, src)
    #define mprGetName(ptr) ""
    #define mprSetAllocName(ptr, name) ptr
    #define mprSetName(ptr, name)
#endif

#define mprAlloc(size) mprSetAllocName(mprAllocFast(size), MPR_LOC)
#define mprMemdup(ptr, size) mprSetAllocName(mprMemdupMem(ptr, size), MPR_LOC)
#define mprRealloc(ptr, size) mprSetAllocName(mprReallocMem(ptr, size), MPR_LOC)
#define mprAllocZeroed(size) mprSetAllocName(mprAllocMem(size, MPR_ALLOC_ZERO), MPR_LOC)
#define mprAllocBlock(size, flags) mprSetAllocName(mprAllocMem(size, flags), MPR_LOC)
#define mprAllocObj(type, manage) ((type*) mprSetManager( \
        mprSetAllocName(mprAllocMem(sizeof(type), MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO), #type "@" MPR_LOC), (MprManager) manage))
#define mprAllocObjWithFlags(type, manage, flags) ((type*) mprSetManager( \
        mprSetAllocName(mprAllocMem(sizeof(type), MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO | flags), #type "@" MPR_LOC), (MprManager) manage))
#define mprAllocStruct(type) ((type*) mprSetAllocName(mprAllocMem(sizeof(type), MPR_ALLOC_ZERO), #type "@" MPR_LOC))

#define mprAllocObjNoZero(type, manage) ((type*) mprSetManager( \
        mprSetAllocName(mprAllocMem(sizeof(type), MPR_ALLOC_MANAGER), #type "@" MPR_LOC), (MprManager) manage))
#define mprAllocStructNoZero(type) ((type*) mprSetAllocName(mprAllocFast(sizeof(type)), #type "@" MPR_LOC))

#if DOXYGEN
typedef void *Type;
/**
    Allocate a block of memory
    @description Allocates a block of memory of the required size. The memory is not zeroed.
    @param size Size of the memory block to allocate.
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion handler
        specified via mprCreate will be called to allow global recovery.
    @remarks Do not mix calls to malloc and mprAlloc.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void *mprAlloc(size_t size);

/**
    Allocate an object of a given type.
    @description Allocates a zeroed block of memory large enough to hold an instance of the specified type with a
        manager callback. This call associates a manager function with an object that will be invoked when the
        object is freed or the garbage collector needs the object to mark internal properties as being used.
        This call is implemented as a macro.
    @param type Type of the object to allocate
    @param manager Manager function to invoke when the allocation is managed.
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion handler
        specified via mprCreate will be called to allow global recovery.
    @remarks Do not mix calls to malloc and mprAlloc.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void *mprAllocObj(Type type, MprManager manager) { return 0;}

/**
    Allocate an object of a given type.
    @description Allocates a zeroed block of memory large enough to hold an instance of the specified type with a
        manager callback. This call associates a manager function with an object that will be invoked when the
        object is freed or the garbage collector needs the object to mark internal properties as being used.
        This call is implemented as a macro.
    This function can be called by foreign (non Mpr) threads provided you use the MPR_ALLOC_HOLD flag so that the memory
    will be preserved until you call mprRelease on the memory block. This is important as without the MPR_ALLOC_HOLD flag,
    the garbage collector could run immediately after calling mprAlloc and collect the memory. When used in an Mpr thread,
    the garbage collector cannot run unless you call mprYield and so the memory is safe from immediate collection.
    @param type Type of the object to allocate
    @param manager Manager function to invoke when the allocation is managed.
    @param flags Allocation flags. Supported flags include: MPR_ALLOC_MANAGER to reserve room for a manager callback and
        MPR_ALLOC_ZERO to zero allocated memory. Use MPR_ALLOC_HOLD to return memory immune from GC. Must use this flag if
        calling from a foreign thread. Use #mprRelease to release back to the system.
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion handler
        specified via mprCreate will be called to allow global recovery.
    @remarks Do not mix calls to malloc and mprAlloc.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void *mprAllocObjWithFlags(Type type, MprManager manager, int flags) { return 0;}

/**
    Allocate a zeroed block of memory
    @description Allocates a zeroed block of memory.
    @param size Size of the memory block to allocate.
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion handler
        specified via mprCreate will be called to allow global recovery.
    @remarks Do not mix calls to malloc and mprAlloc.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void *mprAllocZeroed(size_t size);

#else /* !DOXYGEN */
PUBLIC void *mprAllocMem(size_t size, int flags);
PUBLIC void *mprReallocMem(void *ptr, size_t size);
PUBLIC void *mprMemdupMem(cvoid *ptr, size_t size);
PUBLIC void mprCheckBlock(MprMem *bp);
#endif

/*
    Internal APIs
 */
PUBLIC void mprDestroyMemService(void);
PUBLIC void mprStartGCService(void);
PUBLIC void mprStopGCService(void);
PUBLIC void *mprAllocFast(size_t usize);

/******************************** Garbage Coolector ***************************/
/**
    Add a memory block as a root for garbage collection
    @description Remove the root when no longer required via #mprAddRoot.
    @param ptr Any memory pointer
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC void mprAddRoot(cvoid *ptr);

/*
    Flags for mprGC
 */
#define MPR_CG_DEFAULT      0x0     /**< mprGC flag to run GC if necessary. Will trigger GC and yield. Will
                                         block if GC is required. */
#define MPR_GC_FORCE        0x1     /**< mprGC flag to force start a GC sweep whether it is required or not */
#define MPR_GC_NO_BLOCK     0x2     /**< mprGC flag to run GC if ncessary and return without yielding. Will not block. */
#define MPR_GC_COMPLETE     0x4     /**< mprGC flag to force start a GC and wait until the GC cycle fully completes
                                         including sweep phase */

/**
    Collect garbage
    @description Initiates garbage collection to free unreachable memory blocks.
    It is normally not required for users to invoke this routine as the garbage collector will be scheduled as required.
    If the MPR_GC_NO_BLOCK is not specified, this routine yields to the garbage collector by calling #mprYield.
    Callers must retain all required memory.
    @param flags Flags to control the collection. Set flags to MPR_GC_FORCE to force a collection. Set to MPR_GC_DEFAULT
    to perform a conditional sweep where the sweep is only performed if there is sufficient garbage to warrant a collection.
    Set to MPR_GC_NO_BLOCK to run GC if necessary and return without yielding. Use MPR_GC_COMPLETE to force a GC and wait
    until the GC cycle fully completes including the sweep phase.
    @return The number of blocks freed on the last GC sweep. If using MPR_GC_NO_BLOCK, this may be the result from a prior GC
        sweep.
    @ingroup MprMem
    @stability Stable
  */
PUBLIC int mprGC(int flags);

/**
    Enable or disable the garbage collector
    @param on Set to one to enable and zero to disable.
    @return Returns one if the collector was previously enabled. Otherwise returns zero.
    @ingroup MprMem
    @stability Stable.
 */
PUBLIC bool mprEnableGC(bool on);


/**
    Hold a memory block
    @description This call will protect a memory block from freeing by the garbage collector. Call mprRelease to
        allow the block to be collected.
    @param ptr Any memory block
    @ingroup MprMem
    @stability Stable.
  */
PUBLIC void mprHold(cvoid *ptr);

/**
    Hold memory blocks
    @description This call will protect a set of memory blocks from freeing by the garbage collector.
        Call mprReleaseBlocks to allow the blocks to be collected.
    @param ptr Any memory block
    @param ... Other memory blocks. Terminate the list with a NULL.
    @ingroup MprMem
    @stability Stable
  */
PUBLIC void mprHoldBlocks(cvoid *ptr, ...);

/**
    Release a memory block
    @description This call is used to allow a memory block to be freed by the garbage collector after calling mprHold.
    @param ptr Any memory block
    @ingroup MprMem
    @stability Stable.
  */
PUBLIC void mprRelease(cvoid *ptr);

/**
    Release a memory blocks
    @description This call is used to allow a memory blocks to be freed by the garbage collector after calling mprHoldBlocks.
    @param ptr Any memory block
    @param ... Other memory blocks. Terminate the list with a NULL.
    @ingroup MprMem
    @stability Stable
  */
PUBLIC void mprReleaseBlocks(cvoid *ptr, ...);

/**
    Remove a memory block as a root for garbage collection
    @description The memory block should have previously been added as a root via #mprAddRoot.
    @param ptr Any memory pointer
    @ingroup MprMem
    @stability Stable.
  */
PUBLIC void mprRemoveRoot(cvoid *ptr);

#if DOXYGEN
    /**
        Mark a memory block as in-use
        @description To prevent a memory block being freed by the garbage collector, it must be marked as "active".
            Memory blocks can define a manager that will be invoked by the garbage collector to mark any fields
            that are required by the original block.
        @param ptr Reference to managed memory block. This must be managed memory allocated by the MPR. Do not call
            mprMark on memory allocated via malloc(), strdup() or other non-MPR allocation routines.
            It is safe pass a NULL pointer to mprMark and this will have no effect. This is a convenient pattern
            where manager functions can call mprMark() without testing if the element reference is null or not.
     */
    PUBLIC void mprMark(void *ptr);
    @ingroup MprMem

#else
#if ME_MPR_ALLOC_STATS
    #define HINC(field) MPR->heap->stats.field++
#else
    #define HINC(field)
#endif
    #define mprMark(ptr) \
        if (ptr) { \
            MprMem *_mp = MPR_GET_MEM((ptr)); \
            HINC(markVisited); \
            if (_mp->mark != MPR->heap->mark) { \
                _mp->mark = MPR->heap->mark; \
                if (_mp->hasManager) { \
                    (GET_MANAGER(_mp))((void*) ptr, MPR_MANAGE_MARK); \
                } \
                HINC(marked); \
            } \
        } else {}
#endif

/*
    Internal
 */
PUBLIC int  mprCreateGCService(void);
PUBLIC void mprWakeGCService(void);
PUBLIC void mprResumeThreads(void);
PUBLIC int  mprSyncThreads(MprTicks timeout);

/********************************** Safe Strings ******************************/
/**
    Safe String Module
    @description The MPR provides a suite of safe ascii string manipulation routines to help prevent buffer overflows
        and other potential security traps.
    @defgroup MprString MprString
    @see MprString itos itosradix itosbuf mprEprintf mprPrintf scamel scaselesscmp scaselessmatch schr
        sclone scmp scontains scopy sends sfmt sfmtv shash shashlower sjoin sjoinv slen slower smatch sncaselesscmp snclone
        sncmp sncopy snumber sfnumber shnumber stitle spbrk srchr srejoin srejoinv sreplace sspn sstarts ssub stemplate
        stemplateJson stoi stoiradix stok strim supper sncontains mprFprintf fmtv fmt
    @stability Internal
 */
typedef struct MprString { void *dummy; } MprString;

/**
    Format a string into a static buffer.
    @description This call format a string using printf style formatting arguments. A trailing null will
        always be appended. The call returns the size of the allocated string excluding the null.
    @param buf Pointer to the buffer.
    @param maxSize Size of the buffer.
    @param fmt Printf style format string
    @param ... Variable arguments to format
    @return Returns the buffer.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *fmt(char *buf, ssize maxSize, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Format a string into a statically allocated buffer.
    @description This call format a string using printf style formatting arguments. A trailing null will
        always be appended. The call returns the size of the allocated string excluding the null.
    @param buf Pointer to the buffer.
    @param maxSize Size of the buffer.
    @param fmt Printf style format string
    @param args Varargs argument obtained from va_start.
    @return Returns the buffer;
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *fmtv(char *buf, ssize maxSize, cchar *fmt, va_list args);

/**
    Convert an integer to a string.
    @description This call converts the supplied 64 bit integer to a string using base 10.
    @param value Integer value to convert
    @return An allocated string with the converted number.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *itos(int64 value);

/**
    Convert an integer to a string.
    @description This call converts the supplied 64 bit integer to a string according to the specified radix.
    @param value Integer value to convert
    @param radix The base radix to use when encoding the number
    @return An allocated string with the converted number.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *itosradix(int64 value, int radix);

/**
    Convert an integer to a string buffer.
    @description This call converts the supplied 64 bit integer into a string formatted into the supplied buffer according
        to the specified radix.
    @param buf Pointer to the buffer that will hold the string.
    @param size Size of the buffer.
    @param value Integer value to convert
    @param radix The base radix to use when encoding the number
    @return Returns a reference to the string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *itosbuf(char *buf, ssize size, int64 value, int radix);

/**
    Compare strings ignoring case. This is a safe replacement for strcasecmp. It can handle NULL args.
    @description Compare two strings ignoring case differences. This call operates similarly to strcmp.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @return Returns zero if the strings are equivalent, < 0 if s1 sorts lower than s2 in the collating sequence
        or > 0 if it sorts higher.
    @ingroup MprString
    @stability Stable
 */
PUBLIC int scaselesscmp(cchar *s1, cchar *s2);

/**
    Compare strings ignoring case. This is similar to scaselesscmp but it returns a boolean.
    @description Compare two strings ignoring case differences.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @return Returns true if the strings are equivalent, otherwise false.
    @ingroup MprString
    @stability Stable
 */
PUBLIC bool scaselessmatch(cchar *s1, cchar *s2);

/**
    Create a camel case version of the string
    @description Copy a string into a newly allocated block and make the first character lower case
    @param str Pointer to the block to duplicate.
    @return Returns a newly allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *scamel(cchar *str);

/**
   Find a character in a string.
   @description This is a safe replacement for strchr. It can handle NULL args.
   @param str String to examine
   @param c Character to search for
   @return If the character is found, the call returns a reference to the character position in the string. Otherwise,
        returns NULL.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *schr(cchar *str, int c);

/**
    Clone a string.
    @description Copy a string into a newly allocated block.
    @param str Pointer to the block to duplicate.
    @return Returns a newly allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *sclone(cchar *str);

/**
    Compare strings.
    @description Compare two strings. This is a safe replacement for strcmp. It can handle null args.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @return Returns zero if the strings are identical. Return -1 if the first string is less than the second. Return 1
        if the first string is greater than the second.
    @ingroup MprString
    @stability Stable
 */
PUBLIC int scmp(cchar *s1, cchar *s2);

/**
    Find a pattern in a string.
    @description Locate the first occurrence of pattern in a string.
    @param str Pointer to the string to search.
    @param pattern String pattern to search for.
    @return Returns a reference to the start of the pattern in the string. If not found, returns NULL.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *scontains(cchar *str, cchar *pattern);

/**
    Copy a string.
    @description Safe replacement for strcpy. Copy a string and ensure the destination buffer is not overflowed.
        The call returns the length of the resultant string or an error code if it will not fit into the target
        string. This is similar to strcpy, but it will enforce a maximum size for the copied string and will
        ensure it is always terminated with a null.
    @param dest Pointer to a pointer that will hold the address of the allocated block.
    @param destMax Maximum size of the target string in characters.
    @param src String to copy
    @return Returns the number of characters in the target string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC ssize scopy(char *dest, ssize destMax, cchar *src);

/**
    Test if the string ends with a given pattern.
    @param str String to examine
    @param suffix Pattern to search for
    @return Returns a pointer to the start of the pattern if found. Otherwise returns NULL.
    @ingroup MprString
    @stability Stable
 */
PUBLIC cchar *sends(cchar *str, cchar *suffix);

/**
    Erase the contents of a string
    @param str String to erase
    @ingroup MprString
    @stability Stable
 */
PUBLIC void serase(char *str);

/**
    Format a string. This is a secure verion of printf that can handle null args.
    @description Format the given arguments according to the printf style format. See mprPrintf for a full list of the
        format specifies. This is a secure replacement for sprintf, it can handle null arguments without crashes.
    @param fmt Printf style format string
    @param ... Variable arguments for the format string
    @return Returns a newly allocated string
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *sfmt(cchar *fmt, ...) PRINTF_ATTRIBUTE(1,2);

/**
    Format a string. This is a secure verion of printf that can handle null args.
    @description Format the given arguments according to the printf style format. See mprPrintf for a full list of the
        format specifies. This is a secure replacement for sprintf, it can handle null arguments without crashes.
    @param fmt Printf style format string
    @param args Varargs argument obtained from va_start.
    @return Returns a newly allocated string
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *sfmtv(cchar *fmt, va_list args);

/**
    Compute a hash code for a string
    @param str String to examine
    @param len Length in characters of the string to include in the hash code
    @return Returns an unsigned integer hash code
    @ingroup MprString
    @stability Stable
 */
PUBLIC uint shash(cchar *str, ssize len);

/**
    Compute a caseless hash code for a string
    @description This computes a hash code for the string after converting it to lower case.
    @param str String to examine
    @param len Length in characters of the string to include in the hash code
    @return Returns an unsigned integer hash code
    @ingroup MprString
    @stability Stable
 */
PUBLIC uint shashlower(cchar *str, ssize len);

/**
    Catenate strings.
    @description This catenates strings together with an optional string separator.
        If the separator is NULL, not separator is used. This call accepts a variable list of strings to append,
        terminated by a null argument.
    @param str First string to catentate
    @param ... Variable number of string arguments to append. Terminate list with NULL.
    @return Returns an allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *sjoin(cchar *str, ...);

/**
    Catenate strings.
    @description This catenates strings together with an optional string separator.
        If the separator is NULL, not separator is used. This call accepts a variable list of strings to append,
        terminated by a null argument.
    @param str First string to catentate
    @param args Varargs argument obtained from va_start.
    @return Returns an allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *sjoinv(cchar *str, va_list args);

/**
    Join an array of strings
    @param argc number of strings to join
    @param argv Array of strings
    @param sep Separator string to use. If NULL, then no separator is used.
    @return A single joined string.
    @stability Stable
    @ingroup MprString
 */
PUBLIC cchar *sjoinArgs(int argc, cchar **argv, cchar *sep);

/**
    Return the length of a string.
    @description Safe replacement for strlen. This call returns the length of a string and tests if the length is
        less than a given maximum. It will return zero for NULL args.
    @param str String to measure.
    @return Returns the length of the string
    @ingroup MprString
    @stability Stable
 */
PUBLIC ssize slen(cchar *str);

/**
    Convert a string to lower case.
    @description Convert a string to its lower case equivalent.
    @param str String to convert.
    @return An allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *slower(cchar *str);

/**
    Compare strings.
    @description Compare two strings. This is similar to #scmp but it returns a boolean.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @return Returns true if the strings are equivalent, otherwise false.
    @ingroup MprString
    @stability Stable
 */
PUBLIC bool smatch(cchar *s1, cchar *s2);

/**
    Compare strings ignoring case.
    @description Compare two strings ignoring case differences for a given string length. This call operates
        similarly to strncasecmp.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @param len Length of characters to compare.
    @return Returns zero if the strings are equivalent, < 0 if s1 sorts lower than s2 in the collating sequence
        or > 0 if it sorts higher.
    @ingroup MprString
    @stability Stable
 */
PUBLIC int sncaselesscmp(cchar *s1, cchar *s2, ssize len);

/**
    Clone a substring.
    @description Copy a substring into a newly allocated block.
    @param str Pointer to the block to duplicate.
    @param len Number of bytes to copy. The actual length copied is the minimum of the given length and the length of
        the supplied string. The result is null terminated.
    @return Returns a newly allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *snclone(cchar *str, ssize len);

/**
    Compare strings.
    @description Compare two strings for a given string length. This call operates similarly to strncmp.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @param len Length of characters to compare.
    @return Returns zero if the strings are equivalent, < 0 if s1 sorts lower than s2 in the collating sequence
        or > 0 if it sorts higher.
    @ingroup MprString
    @stability Stable
 */
PUBLIC int sncmp(cchar *s1, cchar *s2, ssize len);

/**
    Find a pattern in a string with a limit.
    @description Locate the first occurrence of pattern in a string, but do not search more than the given character limit.
    @param str Pointer to the string to search.
    @param pattern String pattern to search for.
    @param limit Count of characters in the string to search.
    @return Returns a reference to the start of the pattern in the string. If not found, returns NULL.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *sncontains(cchar *str, cchar *pattern, ssize limit);

/**
    Copy characters from a string.
    @description Safe replacement for strncpy. Copy bytes from a string and ensure the target string is not overflowed.
        The call returns the length of the resultant string or an error code if it will not fit into the target
        string. This is similar to strcpy, but it will enforce a maximum size for the copied string and will
        ensure it is terminated with a null.
    @param dest Pointer to a pointer that will hold the address of the allocated block.
    @param destMax Maximum size of the target string in characters.
    @param src String to copy
    @param len Maximum count of characters to copy
    @return Returns a reference to the destination if successful or NULL if the string won't fit.
    @ingroup MprString
    @stability Stable
 */
PUBLIC ssize sncopy(char *dest, ssize destMax, cchar *src, ssize len);

/*
    Test if a string is a floating point number
    @description The supported format is: [+|-][DIGITS][.][DIGITS][(e|E)[+|-]DIGITS]
    @return true if all characters are digits or '.', 'e', 'E', '+' or '-'
    @ingroup MprString
    @stability Stable
 */
PUBLIC bool sfnumber(cchar *s);

/*
    Test if a string is a hexadecimal number
    @description The supported format is: [(+|-)][0][(x|X)][HEX_DIGITS]
    @return true if all characters are digits or 'x' or 'X'
    @ingroup MprString
    @stability Stable
 */
PUBLIC bool shnumber(cchar *s);

/*
    Test if a string is a radix 10 number.
    @description The supported format is: [(+|-)][DIGITS]
    @return true if all characters are digits or '+' or '-'
    @ingroup MprString
    @stability Stable
 */
PUBLIC bool snumber(cchar *s);

/**
    Create a Title Case version of the string
    @description Copy a string into a newly allocated block and make the first character upper case
    @param str Pointer to the block to duplicate.
    @return Returns a newly allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *stitle(cchar *str);

/**
    Locate the a character from a set in a string.
    @description This locates in the string the first occurence of any character from a given set of characters.
    @param str String to examine
    @param set Set of characters to scan for
    @return Returns a reference to the first character from the given set. Returns NULL if none found.
    @ingroup MprString
    @stability Stable
  */
PUBLIC char *spbrk(cchar *str, cchar *set);

/**
    Find a character in a string by searching backwards.
    @description This locates in the string the last occurence of a character.
    @param str String to examine
    @param c Character to scan for
    @return Returns a reference in the string to the requested character. Returns NULL if none found.
    @ingroup MprString
    @stability Stable
  */
PUBLIC char *srchr(cchar *str, int c);

/**
    Append strings to an existing string and reallocate as required.
    @description Append a list of strings to an existing string. The list of strings is terminated by a
        null argument. The call returns the size of the allocated block.
    @param buf Existing (allocated) string to reallocate. May be null. May not be a string literal.
    @param ... Variable number of string arguments to append. Terminate list with NULL
    @return Returns an allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *srejoin(char *buf, ...);

/**
    Append strings to an existing string and reallocate as required.
    @description Append a list of strings to an existing string. The list of strings is terminated by a
        null argument. The call returns the size of the allocated block.
    @param buf Existing (allocated) string to reallocate. May be null. May not be a string literal.
    @param args Varargs argument obtained from va_start.
    @return Returns an allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *srejoinv(char *buf, va_list args);

/*
    Replace a pattern in a string
    @description This will replace all occurrences of the pattern in the string.
    @param str String to examine
    @param pattern Pattern to search for. Can be null in which case the str is cloned.
    @param replacement Replacement pattern. If replacement is null, the pattern is removed.
    @return A new allocated string
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *sreplace(cchar *str, cchar *pattern, cchar *replacement);

/*
    Test if a string is all white space
    @return true if all characters are ' ', '\t', '\n', '\r'. True if the string is empty.
    @ingroup MprString
    @stability Stable
 */
PUBLIC bool sspace(cchar *s);

/**
    Split a string at a delimiter
    @description Split a string and return parts. The string is modified.
        This routiner never returns null. If there are leading delimiters, the empty string will be returned
        and *last will be set to the portion after the delimiters.
        If str is null, a managed reference to the empty string will be returned.
        If there are no characters after the delimiter, then *last will be set to the empty string.
    @param str String to tokenize.
    @param delim Set of characters that are used as token separators.
    @param last Reference to the portion after the delimiters. Will return an empty string if is not trailing portion.
    @return Returns a pointer to the first part before the delimiters. If the string begins with delimiters, the empty
        string will be returned.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *ssplit(char *str, cchar *delim, char **last);

/**
    Find the end of a spanning prefix
    @description This scans the given string for characters from the set and returns an index to the first character not in the set.
    @param str String to examine
    @param set Set of characters to span
    @return Returns an index to the first character after the spanning set. If not found, returns the index of the first null.
    @ingroup MprString
    @stability Stable
  */
PUBLIC ssize sspn(cchar *str, cchar *set);

/**
    Test if the string starts with a given pattern.
    @param str String to examine
    @param prefix Pattern to search for
    @return Returns TRUE if the pattern was found. Otherwise returns zero.
    @ingroup MprString
    @stability Stable
 */
PUBLIC bool sstarts(cchar *str, cchar *prefix);

/**
    Replace template tokens in a string with values from a lookup table. Tokens are ${variable} references.
    @param str String to expand
    @param tokens Hash table of token values to use
    @return An expanded string. May return the original string if no "$" references are present.
    @ingroup MprString
    @stability Stable
    @see stemplateJson
  */
PUBLIC char *stemplate(cchar *str, struct MprHash *tokens);

/**
    Replace template tokens in a string with values from a lookup table. Tokens are ${variable} references.
    @param str String to expand
    @param tokens Json object of token values to use
    @return An expanded string. May return the original string if no "$" references are present.
    @ingroup MprString
    @stability Stable
    @see stemplate
  */
PUBLIC char *stemplateJson(cchar *str, struct MprJson *tokens);

/**
    Convert a string to a double.
    @description This call converts the supplied string to a double.
    @param str Pointer to the string to parse.
    @return Returns the double equivalent value of the string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC double stof(cchar *str);

/**
    Convert a string to an integer.
    @description This call converts the supplied string to an integer using base 10.
    @param str Pointer to the string to parse.
    @return Returns the integer equivalent value of the string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC int64 stoi(cchar *str);

/**
    Convert a string to an integer.
    @description This call converts the supplied string to an integer using the specified radix (base).
    @param str Pointer to the string to parse.
    @param radix Base to use when parsing the string
    @param err Return error code. Set to 0 if successful.
    @return Returns the integer equivalent value of the string.
    @ingroup MprString
 */
PUBLIC int64 stoiradix(cchar *str, int radix, int *err);

/**
    Tokenize a string
    @description Split a string into tokens using a character set as delimiters.
    @param str String to tokenize.
    @param delim Set of characters that are used as token separators.
    @param last Last token pointer.
    @return Returns a pointer to the next token.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *stok(char *str, cchar *delim, char **last);

/**
    Tokenize a string
    @description Split a string into tokens using a string pattern as delimiters.
    @param str String to tokenize.
    @param pattern String pattern to use for token delimiters.
    @param last Last token pointer.
    @return Returns a pointer to the next token.
    @ingroup MprString
    @stability Stable
*/
PUBLIC char *sptok(char *str, cchar *pattern, char **last);

/**
   String to list. This parses the string into space separated arguments. Single and double quotes are supported.
   @param src Source string to parse
   @return List of arguments
   @ingroup MprString
   @stability Stable
 */
PUBLIC struct MprList *stolist(cchar *src);

/**
    Create a substring
    @param str String to examine
    @param offset Starting offset within str for the beginning of the substring
    @param length Length of the substring in characters
    @return Returns a newly allocated substring
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *ssub(cchar *str, ssize offset, ssize length);

/*
    String trim flags
 */
#define MPR_TRIM_START  0x1             /**< Flag for #strim to trim from the start of the string */
#define MPR_TRIM_END    0x2             /**< Flag for #strim to trim from the end of the string */
#define MPR_TRIM_BOTH   0x3             /**< Flag for #strim to trim from both the start and the end of the string */

/**
    Trim a string.
    @description Trim leading and trailing characters off a string.
    The original string is not modified and the return value is a newly allocated string.
    @param str String to trim.
    @param set String of characters to remove.
    @param where Flags to indicate trim from the start, end or both. Use MPR_TRIM_START, MPR_TRIM_END, MPR_TRIM_BOTH.
    @return Returns a newly allocated trimmed string. May not equal \a str.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *strim(cchar *str, cchar *set, int where);

/**
    Convert a string to upper case.
    @description Convert a string to its upper case equivalent.
    @param str String to convert.
    @return Returns a pointer to an allocated string.
    @ingroup MprString
    @stability Stable
 */
PUBLIC char *supper(cchar *str);

/************************************ Unicode *********************************/
/*
    Low-level unicode wide string support. Unicode characters are build-time configurable to be 1, 2 or 4 bytes

    This API is not yet public
 */
/* Allocating */
PUBLIC wchar   *amtow(cchar *src, ssize *len);
PUBLIC char    *awtom(wchar *src, ssize *len);

#if ME_CHAR_LEN > 1
#define multi(s) awtom(s, 0)
#define wide(s)  amtow(s, 0)
#else
#define multi(s) (s)
#define wide(s)  (s)
#endif

#if ME_CHAR_LEN > 1
PUBLIC ssize   wtom(char *dest, ssize count, wchar *src, ssize len);
PUBLIC ssize   mtow(wchar *dest, ssize count, cchar *src, ssize len);

#if KEEP
PUBLIC wchar    *wfmt(wchar *fmt, ...);
PUBLIC wchar    *itow(wchar *buf, ssize bufCount, int64 value, int radix);
PUBLIC wchar    *wchr(wchar *s, int c);
PUBLIC int      wcasecmp(wchar *s1, wchar *s2);
PUBLIC wchar    *wclone(wchar *str);
PUBLIC int      wcmp(wchar *s1, wchar *s2);
PUBLIC wchar    *wcontains(wchar *str, wchar *pattern, ssize limit);
PUBLIC ssize    wcopy(wchar *dest, ssize destMax, wchar *src);
PUBLIC int      wends(wchar *str, wchar *suffix);
PUBLIC wchar    *wfmtv(wchar *fmt, va_list arg);
PUBLIC uint     whash(wchar *name, ssize len);
PUBLIC uint     whashlower(wchar *name, ssize len);
PUBLIC wchar    *wjoin(wchar *sep, ...);
PUBLIC wchar    *wjoinv(wchar *sep, va_list args);
PUBLIC ssize    wlen(wchar *s);
#endif

PUBLIC wchar    *wlower(wchar *s);
PUBLIC int      wncaselesscmp(wchar *s1, wchar *s2, ssize len);
PUBLIC int      wncmp(wchar *s1, wchar *s2, ssize len);
PUBLIC ssize    wncopy(wchar *dest, ssize destCount, wchar *src, ssize len);
PUBLIC wchar    *wpbrk(wchar *str, wchar *set);
PUBLIC wchar    *wrchr(wchar *s, int c);
PUBLIC wchar    *wrejoin(wchar *buf, wchar *sep, ...);
PUBLIC wchar    *wrejoinv(wchar *buf, wchar *sep, va_list args);
PUBLIC ssize    wspn(wchar *str, wchar *set);
PUBLIC int      wstarts(wchar *str, wchar *prefix);
PUBLIC wchar    *wsub(wchar *str, ssize offset, ssize len);
PUBLIC int64    wtoi(wchar *str);
PUBLIC int64    wtoiradix(wchar *str, int radix, int *err);
PUBLIC wchar    *wtok(wchar *str, wchar *delim, wchar **last);
PUBLIC wchar    *wtrim(wchar *str, wchar *set, int where);
PUBLIC wchar    *wupper(wchar *s);

#else

/* CHAR_LEN == 1 */

#define wtom(dest, count, src, len)         sncopy(dest, count, src, len)
#define mtow(dest, count, src, len)         sncopy(dest, count, src, len)
#define itowbuf(buf, bufCount, value, radix) itosbuf(buf, bufCount, value, radix)
#define wchr(str, c)                        schr(str, c)
#define wclone(str)                         sclone(str)
#define wcasecmp(s1, s2)                    scaselesscmp(s1, s2)
#define wcmp(s1, s2)                        scmp(s1, s2)
#define wcontains(str, pattern)             scontains(str, pattern)
#define wncontains(str, pattern, limit)     sncontains(str, pattern, limit)
#define wcopy(dest, count, src)             scopy(dest, count, src)
#define wends(str, suffix)                  sends(str, suffix)
#define wfmt                                sfmt
#define wfmtv(fmt, arg)                     sfmtv(fmt, arg)
#define whash(name, len)                    shash(name, len)
#define whashlower(name, len)               shashlower(name, len)
#define wjoin                               sjoin
#define wjoinv(sep, args)                   sjoinv(sep, args)
#define wlen(str)                           slen(str)
#define wlower(str)                         slower(str)
#define wncmp(s1, s2, len)                  sncmp(s1, s2, len)
#define wncaselesscmp(s1, s2, len)          sncaselesscmp(s1, s2, len)
#define wncopy(dest, count, src, len)       sncopy(dest, count, src, len)
#define wpbrk(str, set)                     spbrk(str, set)
#define wrchr(str, c)                       srchr(str, c)
#define wrejoin                             srejoin
#define wrejoinv(buf, sep, args)            srejoinv(buf, sep, args)
#define wspn(str, set)                      sspn(str, set)
#define wstarts(str, prefix)                sstarts(str, prefix)
#define wsub(str, offset, len)              ssub(str, offset, len)
#define wtoi(str)                           stoi(str)
#define wtoiradix(str, radix, err)          stoiradix(str, radix, err)
#define wtok(str, delim, last)              stok(str, delim, last)
#define wtrim(str, set, where)              strim(str, set, where)
#define wupper(str)                         supper(str)

#endif /* ME_CHAR_LEN > 1 */

/********************************* Mixed Strings ******************************/
/*
    These routines operate on wide strings mixed with a multibyte/ascii operand
    This API is not yet public
 */
#if ME_CHAR_LEN > 1
#if KEEP
PUBLIC int      mcaselesscmp(wchar *s1, cchar *s2);
PUBLIC int      mcmp(wchar *s1, cchar *s2);
PUBLIC wchar    *mcontains(wchar *str, cchar *pattern);
PUBLIC wchar    *mncontains(wchar *str, cchar *pattern, ssize limit);
PUBLIC ssize    mcopy(wchar *dest, ssize destMax, cchar *src);
PUBLIC int      mends(wchar *str, cchar *suffix);
PUBLIC wchar    *mfmt(cchar *fmt, ...);
PUBLIC wchar    *mfmtv(cchar *fmt, va_list arg);
PUBLIC wchar    *mjoin(cchar *str, ...);
PUBLIC wchar    *mjoinv(wchar *buf, va_list args);
PUBLIC int      mncmp(wchar *s1, cchar *s2, ssize len);
PUBLIC int      mncaselesscmp(wchar *s1, cchar *s2, ssize len);
PUBLIC ssize    mncopy(wchar *dest, ssize destMax, cchar *src, ssize len);
PUBLIC wchar    *mpbrk(wchar *str, cchar *set);
PUBLIC wchar    *mrejoin(wchar *buf, cchar *sep, ...);
PUBLIC wchar    *mrejoinv(wchar *buf, cchar *sep, va_list args);
PUBLIC ssize    mspn(wchar *str, cchar *set);
PUBLIC int      mstarts(wchar *str, cchar *prefix);
PUBLIC wchar    *mtok(wchar *str, cchar *delim, wchar **last);
PUBLIC wchar    *mtrim(wchar *str, cchar *set, int where);
#endif

#else /* ME_CHAR_LEN <= 1 */

#define mcaselesscmp(s1, s2)            scaselesscmp(s1, s2)
#define mcmp(s1, s2)                    scmp(s1, s2)
#define mcontains(str, pattern)         scontains(str, pattern)
#define mncontains(str, pattern, limit) sncontains(str, pattern, limit)
#define mcopy(dest, count, src)         scopy(dest, count, src)
#define mends(str, suffix)              sends(str, suffix)
#define mfmt                            sfmt
#define mfmtv(fmt, arg)                 sfmtv(fmt, arg)
#define mjoin                           sjoin
#define mjoinv(sep, args)               sjoinv(sep, args)
#define mncmp(s1, s2, len)              sncmp(s1, s2, len)
#define mncaselesscmp(s1, s2, len)      sncaselesscmp(s1, s2, len)
#define mncopy(dest, count, src, len)   sncopy(dest, count, src, len)
#define mpbrk(str, set)                 spbrk(str, set)
#define mrejoin                         srejoin
#define mrejoinv(buf, sep, args)        srejoinv(buf, sep, args)
#define mspn(str, set)                  sspn(str, set)
#define mstarts(str, prefix)            sstarts(str, prefix)
#define mtok(str, delim, last)          stok(str, delim, last)
#define mtrim(str, set, where)          strim(str, set, where)

#endif /* ME_CHAR_LEN <= 1 */

/************************************ Formatting ******************************/
/**
    Print a formatted message to the standard error channel
    @description This is a secure replacement for fprintf(stderr).
    @param fmt Printf style format string
    @param ... Variable arguments to format
    @return Returns the number of bytes written
    @ingroup MprString
    @stability Stable
 */
PUBLIC ssize mprEprintf(cchar *fmt, ...) PRINTF_ATTRIBUTE(1,2);

/**
    Print a formatted message to a file descriptor
    @description This is a replacement for fprintf as part of the safe string MPR library. It minimizes
        memory use and uses a file descriptor instead of a File pointer.
    @param file MprFile object returned via #mprOpenFile.
    @param fmt Printf style format string
    @param ... Variable arguments to format
    @return Returns the number of bytes written
    @ingroup MprString
    @stability Stable
 */
PUBLIC ssize mprFprintf(struct MprFile *file, cchar *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/**
    Formatted print. This is a secure verion of printf that can handle null args.
    @description This is a secure replacement for printf. It can handle null arguments without crashes.
    @param fmt Printf style format string
    @param ... Variable arguments to format
    @return Returns the number of bytes written
    @ingroup MprString
    @stability Stable
 */
PUBLIC ssize mprPrintf(cchar *fmt, ...) PRINTF_ATTRIBUTE(1, 2);

/**
    Print to stdout and add a trailing newline
    @internal
 */
PUBLIC ssize print(cchar *fmt, ...) PRINTF_ATTRIBUTE(1,2);

/**
    Format a string into a buffer.
    @description This routine will format the arguments into a result. If a buffer is supplied, it will be used.
        Otherwise if the buf argument is NULL, a buffer will be allocated. The arguments will be formatted up
        to the maximum size supplied by the maxsize argument.  A trailing null will always be appended.
    @param buf Optional buffer to contain the formatted result
    @param maxsize Maximum size of the result
    @param fmt Printf style format string
    @param args Variable arguments to format
    @return Returns the number of characters in the string.
    @ingroup MprString
    @internal
    @stability Stable
 */
PUBLIC char *mprPrintfCore(char *buf, ssize maxsize, cchar *fmt, va_list args);

/********************************* Floating Point *****************************/
#if ME_FLOAT
/**
    Floating Point Services
    @stability Stable
    @see mprDota mprIsInfinite mprIsNan mprIsZero
    @defgroup MprFloat MprFloat
    @stability Internal
  */
typedef struct MprFloat { int dummy; } MprFloat;

/**
    Test if a double value is infinte
    @param value Value to test
    @return True if the value is +Infinity or -Infinity
    @ingroup MprFloat
    @stability Stable
 */
PUBLIC int mprIsInfinite(double value);

/**
    Test if a double value is zero
    @param value Value to test
    @return True if the value is zero
    @ingroup MprFloat
    @stability Stable
 */
PUBLIC int mprIsZero(double value);

/**
    Test if a double value is not-a-number
    @param value Value to test
    @return True if the value is NaN
    @ingroup MprFloat
    @stability Stable
 */
PUBLIC int mprIsNan(double value);

#endif /* ME_FLOAT */
/********************************* Buffering **********************************/
/**
    Buffer refill callback function
    @description Function to call when the buffer is depleted and needs more data.
    @param buf Instance of an MprBuf
    @param arg Data argument supplied to #mprSetBufRefillProc
    @returns The callback should return 0 if successful, otherwise a negative error code.
    @ingroup MprBuf
    @stability Stable
 */
typedef int (*MprBufProc)(struct MprBuf* bp, void *arg);

/**
    Dynamic Buffer Module
    @description MprBuf is a flexible, dynamic growable buffer structure. It has start and end pointers to the
        data buffer which act as read/write pointers. Routines are provided to get and put data into and out of the
        buffer and automatically advance the appropriate start/end pointer. By definition, the buffer is empty when
        the start pointer == the end pointer. Buffers can be created with a fixed size or can grow dynamically as
        more data is added to the buffer.
    \n\n
    For performance, the specification of MprBuf is deliberately exposed. All members of MprBuf are implicitly public.
    However, it is still recommended that wherever possible, you use the accessor routines provided.
    @see MprBuf MprBufProc mprAddNullToBuf mprAddNullToWideBuf mprAdjustBufEnd mprAdjustBufStart mprBufToString mprCloneBuf
        mprCompactBuf mprCreateBuf mprFlushBuf mprGetBlockFromBuf mprGetBufEnd mprGetBufLength mprGetBufOrigin
        mprGetBufRefillProc mprGetBufSize mprGetBufSpace mprGetBufStart mprGetCharFromBuf mprGrowBuf
        mprInsertCharToBuf mprLookAtLastCharInBuf mprLookAtNextCharInBuf mprPutBlockToBuf mprPutCharToBuf
        mprPutCharToWideBuf mprPutToBuf mprPutFmtToWideBuf mprPutIntToBuf mprPutPadToBuf mprPutStringToBuf
        mprPutStringToWideBuf mprPutSubStringToBuf mprRefillBuf mprResetBufIfEmpty mprSetBufMax mprSetBufRefillProc
        mprSetBufSize
    @defgroup MprBuf MprBuf
    @stability Internal.
 */
typedef struct MprBuf {
    char            *data;              /**< Actual buffer for data */
    char            *endbuf;            /**< Pointer one past the end of buffer */
    char            *start;             /**< Pointer to next data char */
    char            *end;               /**< Pointer one past the last data chr */
    ssize           buflen;             /**< Current size of buffer */
    ssize           maxsize;            /**< Max size the buffer can ever grow */
    ssize           growBy;             /**< Next growth increment to use */
    MprBufProc      refillProc;         /**< Auto-refill procedure */
    void            *refillArg;         /**< Refill arg - must be alloced memory */
} MprBuf;

/**
    Add a null character to the buffer contents.
    @description Add a null byte but do not change the buffer content lengths. The null is added outside the
        "official" content length. This is useful when calling #mprGetBufStart and using the returned pointer
        as a "C" string pointer.
    @param buf Buffer created via mprCreateBuf
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC void mprAddNullToBuf(MprBuf *buf);

/**
    Adjust the buffer end position
    @description Adjust the buffer end position by the specified amount. This is typically used to advance the
        end position as content is appended to the buffer. Adjusting the start or end position will change the value
        returned by #mprGetBufLength. If using the mprPutBlock or mprPutChar routines, adjusting the end position is
        done automatically.
    @param buf Buffer created via mprCreateBuf
    @param count Positive or negative count of bytes to adjust the end position.
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC void mprAdjustBufEnd(MprBuf *buf, ssize count);

/**
    Adjust the buffer start position
    @description Adjust the buffer start position by the specified amount. This is typically used to advance the
        start position as content is consumed. Adjusting the start or end position will change the value returned
        by #mprGetBufLength. If using the mprGetBlock or mprGetChar routines, adjusting the start position is
        done automatically.
    @param buf Buffer created via mprCreateBuf
    @param count Positive or negative count of bytes to adjust the start position.
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC void mprAdjustBufStart(MprBuf *buf, ssize count);

/**
    Convert the buffer contents to a string
    @param buf Buffer created via mprCreateBuf
    @returns Allocated string
    @ingroup MprBuf
    @stability Stable.
*/
PUBLIC char *mprBufToString(MprBuf *buf);

/**
    Create a new buffer
    @description Create a new buffer.
    @param initialSize Initial size of the buffer
    @param maxSize Maximum size the buffer can grow to
    @return a new buffer
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC MprBuf *mprCreateBuf(ssize initialSize, ssize maxSize);

/**
    Clone a buffer
    @description Copy the buffer and contents into a newly allocated buffer
    @param orig Original buffer to copy
    @return Returns a newly allocated buffer
    @stability Stable.
 */
PUBLIC MprBuf *mprCloneBuf(MprBuf *orig);

/**
    Clone a buffer contents
    @param bp Buffer to copy
    @return Returns a newly allocated memory block containing the buffer contents.
    @stability Stable.
 */
PUBLIC char *mprCloneBufMem(MprBuf *bp);

/**
    Clone a buffer contents
    @param bp Buffer to copy
    @return Returns a string containing the buffer contents.
    @stability Stable.
 */
PUBLIC char *mprCloneBufAsString(MprBuf *bp);

/**
    Compact the buffer contents
    @description Compact the buffer contents by copying the contents down to start the the buffer origin.
    @param buf Buffer created via mprCreateBuf
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC void mprCompactBuf(MprBuf *buf);

/**
    Flush the buffer contents
    @description Discard the buffer contents and reset the start end content pointers.
    @param buf Buffer created via mprCreateBuf
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC void mprFlushBuf(MprBuf *buf);

/**
    Get a block of data from the buffer
    @description Get a block of data from the buffer start and advance the start position. If the requested
        length is greater than the available buffer content, then return whatever data is available.
    @param buf Buffer created via mprCreateBuf
    @param blk Destination block for the read data.
    @param count Count of bytes to read from the buffer.
    @return The count of bytes read into the block or -1 if the buffer is empty.
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC ssize mprGetBlockFromBuf(MprBuf *buf, char *blk, ssize count);

/**
    Get a reference to the end of the buffer contents
    @description Get a pointer to the location immediately after the end of the buffer contents.
    @param buf Buffer created via mprCreateBuf
    @returns Pointer to the end of the buffer data contents. Points to the location one after the last data byte.
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC char *mprGetBufEnd(MprBuf *buf);

/**
    Get the buffer content length.
    @description Get the length of the buffer contents. This is not the same as the buffer size which may be larger.
    @param buf Buffer created via mprCreateBuf
    @returns The length of the content stored in the buffer in bytes
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC ssize mprGetBufLength(MprBuf *buf);

/**
    Get the buffer refill procedure
    @description Return the buffer refill callback function.
    @param buf Buffer created via mprCreateBuf
    @returns The refill call back function if defined.
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC MprBufProc mprGetBufRefillProc(MprBuf *buf);

/**
    Get the origin of the buffer content storage.
    @description Get a pointer to the start of the buffer content storage. This is always and allocated block.
    @param buf Buffer created via mprCreateBuf
    @returns A pointer to the buffer content storage.
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC char *mprGetBuf(MprBuf *buf);

/**
    Get the current size of the buffer content storage.
    @description This returns the size of the memory block allocated for storing the buffer contents.
    @param buf Buffer created via mprCreateBuf
    @returns The size of the buffer content storage.
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC ssize mprGetBufSize(MprBuf *buf);

/**
    Get the space available to store content
    @description Get the number of bytes available to store content in the buffer
    @param buf Buffer created via mprCreateBuf
    @returns The number of bytes available
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC ssize mprGetBufSpace(MprBuf *buf);

/**
    Get the start of the buffer contents
    @description Get a pointer to the start of the buffer contents. Use #mprGetBufLength to determine the length
        of the content. Use #mprGetBufEnd to get a pointer to the location after the end of the content.
    @param buf Buffer created via mprCreateBuf
    @returns Pointer to the start of the buffer data contents
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC char *mprGetBufStart(MprBuf *buf);

/**
    Get a character from the buffer
    @description Get the next byte from the buffer start and advance the start position.
    @param buf Buffer created via mprCreateBuf
    @return The character or -1 if the buffer is empty.
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC int mprGetCharFromBuf(MprBuf *buf);

/**
    Grow the buffer
    @description Grow the storage allocated for content for the buffer. The new size must be less than the maximum
        limit specified via #mprCreateBuf or #mprSetBufSize.
    @param buf Buffer created via mprCreateBuf
    @param count Count of bytes by which to grow the buffer content size.
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC int mprGrowBuf(MprBuf *buf, ssize count);

/**
    Insert a character into the buffer
    @description Insert a character into to the buffer prior to the current buffer start point.
    @param buf Buffer created via mprCreateBuf
    @param c Character to append.
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC int mprInsertCharToBuf(MprBuf *buf, int c);

/**
    Peek at the next character in the buffer
    @description Non-destructively return the next character from the start position in the buffer.
        The character is returned and the start position is not altered.
    @param buf Buffer created via mprCreateBuf
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC int mprLookAtNextCharInBuf(MprBuf *buf);

/**
    Peek at the last character in the buffer
    @description Non-destructively return the last character from just prior to the end position in the buffer.
        The character is returned and the end position is not altered.
    @param buf Buffer created via mprCreateBuf
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC int mprLookAtLastCharInBuf(MprBuf *buf);

/**
    Put a block to the buffer.
    @description Append a block of data  to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param ptr Block to append
    @param size Size of block to append
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC ssize mprPutBlockToBuf(MprBuf *buf, cchar *ptr, ssize size);

/**
    Put a character to the buffer.
    @description Append a character to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param c Character to append
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC int mprPutCharToBuf(MprBuf *buf, int c);

/**
    Put a formatted string to the buffer.
    @description Format a string and append to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param fmt Printf style format string
    @param ... Variable arguments for the format string
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC ssize mprPutToBuf(MprBuf *buf, cchar *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/**
    Put an integer to the buffer.
    @description Append a integer to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param i Integer to append to the buffer
    @returns Number of characters added to the buffer, otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC ssize mprPutIntToBuf(MprBuf *buf, int64 i);

/**
    Put padding characters to the buffer.
    @description Append padding characters to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param c Character to append
    @param count Count of pad characters to put
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable
 */
PUBLIC ssize mprPutPadToBuf(MprBuf *buf, int c, ssize count);

/**
    Put a string to the buffer.
    @description Append a null terminated string to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param str String to append
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC ssize mprPutStringToBuf(MprBuf *buf, cchar *str);

/**
    Put a substring to the buffer.
    @description Append a null terminated substring to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param str String to append
    @param count Put at most count characters to the buffer
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC ssize mprPutSubStringToBuf(MprBuf *buf, cchar *str, ssize count);

/**
    Refill the buffer with data
    @description Refill the buffer by calling the refill procedure specified via #mprSetBufRefillProc
    @param buf Buffer created via mprCreateBuf
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC int mprRefillBuf(MprBuf *buf);

/**
    Reset the buffer
    @description If the buffer is empty, reset the buffer start and end pointers to the beginning of the buffer.
    @param buf Buffer created via mprCreateBuf
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC void mprResetBufIfEmpty(MprBuf *buf);

/**
    Set the maximum buffer size
    @description Update the maximum buffer size set when the buffer was created
    @param buf Buffer created via mprCreateBuf
    @param maxSize New maximum size the buffer can grow to
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC void mprSetBufMax(MprBuf *buf, ssize maxSize);

/**
    Set the buffer refill procedure
    @description Define a buffer refill procedure. The MprBuf module will not invoke or manage this refill procedure.
        It is simply stored to allow upper layers to use and provide their own auto-refill mechanism.
    @param buf Buffer created via mprCreateBuf
    @param fn Callback function to store.
    @param arg Callback data argument.
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC void mprSetBufRefillProc(MprBuf *buf, MprBufProc fn, void *arg);

/**
    Set the buffer size
    @description Set the current buffer content size and maximum size limit. Setting a current size will
        immediately grow the buffer to be this size. If the size is less than the current buffer size,
        the requested size will be ignored. ie. this call will not shrink the buffer. Setting a maxSize
        will define a maximum limit for how big the buffer contents can grow. Set either argument to
        -1 to be ignored.
    @param buf Buffer created via mprCreateBuf
    @param size Size to immediately make the buffer. If size is less than the current buffer size, it will be ignored.
        Set to -1 to ignore this parameter.
    @param maxSize Maximum size the buffer contents can grow to.
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Stable.
 */
PUBLIC int mprSetBufSize(MprBuf *buf, ssize size, ssize maxSize);

#if DOXYGEN || ME_CHAR_LEN > 1
#if KEEP
/**
    Add a wide null character to the buffer contents.
    @description Add a null character but do not change the buffer content lengths. The null is added outside the
        "official" content length. This is useful when calling #mprGetBufStart and using the returned pointer
        as a string pointer.
    @param buf Buffer created via mprCreateBuf
    @ingroup MprBuf
    @stability Prototype
  */
PUBLIC void mprAddNullToWideBuf(MprBuf *buf);

/**
    Put a wide character to the buffer.
    @description Append a wide character to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param c Character to append
    @returns Zero if successful and otherwise a negative error code
    @ingroup MprBuf
    @stability Prototype
  */
PUBLIC int mprPutCharToWideBuf(MprBuf *buf, int c);

/**
    Put a wide string to the buffer.
    @description Append a null terminated wide string to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param str String to append
    @returns Count of bytes written and otherwise a negative error code
    @ingroup MprBuf
    @stability Prototype
*/
PUBLIC ssize mprPutStringToWideBuf(MprBuf *buf, cchar *str);

/**
    Put a formatted wide string to the buffer.
    @description Format a string and append to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via mprCreateBuf
    @param fmt Printf style format string
    @param ... Variable arguments for the format string
    @returns Count of bytes written and otherwise a negative error code
    @ingroup MprBuf
    @stability Prototype
 */
PUBLIC ssize mprPutFmtToWideBuf(MprBuf *buf, cchar *fmt, ...) PRINTF_ATTRIBUTE(2,3);

#endif /* KEEP */
#else /* ME_CHAR_LEN == 1 */

#define mprAddNullToWideBuf     mprAddNullToBuf
#define mprPutCharToWideBuf     mprPutCharToBuf
#define mprPutStringToWideBuf   mprPutStringToBuf
#define mprPutFmtToWideBuf      mprPutToBuf
#endif

/*
    Macros for speed
 */
#define mprGetBufLength(bp) ((ssize) ((bp)->end - (bp)->start))
#define mprGetBufSize(bp) ((bp)->buflen)
#define mprGetBufSpace(bp) ((bp)->endbuf - (bp)->end)
#define mprGetBuf(bp) ((bp)->data)
#define mprGetBufStart(bp) ((bp)->start)
#define mprGetBufEnd(bp) ((bp)->end)

/*
    Prototype
 */
PUBLIC uint mprGetUint16FromBuf(MprBuf *buf);
PUBLIC uint mprGetUint24FromBuf(MprBuf *buf);
PUBLIC uint mprGetUint32FromBuf(MprBuf *buf);
PUBLIC uint mprPeekUint32FromBuf(MprBuf *buf);
PUBLIC void mprPutUint16ToBuf(MprBuf *buf, uint16 num);
PUBLIC void mprPutUint32ToBuf(MprBuf *buf, uint32 num);

/******************************** Date and Time *******************************/
/**
    Format a date according to RFC822: (Fri, 07 Jan 2003 12:12:21 PDT)
 */
#define MPR_RFC_DATE        "%a, %d %b %Y %T %Z"
#define MPR_RFC822_DATE     "%a, %d %b %Y %T %Z"

/**
    Default date format used in mprFormatLocalTime/mprFormatUniversalTime when no format supplied
 */
#define MPR_DEFAULT_DATE    "%a %b %d %T %Y %Z"

/**
    Date format for use in HTTP (headers)
 */
#define MPR_HTTP_DATE       "%a, %d %b %Y %T GMT"

/**
    Date format for RFC 3399 for use in HTML 5
 */
#define MPR_RFC3399_DATE    "%FT%TZ"

/**
    Date for use in log files (compact)
 */
#define MPR_LOG_DATE        "%D %T"

/********************************** Defines ***********************************/
/**
    Date and Time Service
    @stability Stable
    @see MprTime mprCompareTime mprCreateTimeService mprDecodeLocalTime mprDecodeUniversalTime mprFormatLocalTime
        mprFormatTm mprGetDate mprGetElapsedTicks mprGetRemainingTicks mprGetHiResTicks mprGetTimeZoneOffset mprMakeTime
        mprMakeUniversalTime mprParseTime
    @defgroup MprTime MprTime
 */
typedef Time MprTime;

/**
    Mpr time structure.
    @description MprTime is the cross platform time abstraction structure. Time is stored as milliseconds
        since the epoch: 00:00:00 UTC Jan 1 1970. MprTime is typically a 64 bit quantity.
    @ingroup MprTime
    @stability Internal
 */
PUBLIC int mprCreateTimeService(void);

/**
    Compare two times
    @description compare two times and return a code indicating which is greater, less or equal
    @param t1 First time
    @param t2 Second time
    @returns Zero if equal, -1 if t1 is less than t2 otherwise one.
    @ingroup MprTime
    @stability Stable
 */
PUBLIC int mprCompareTime(MprTime t1, MprTime t2);

/**
    Decode a time value into a tokenized local time value.
    @description Safe replacement for localtime. This call converts the time value to local time and formats
        the as a struct tm.
    @param timep Pointer to a tm structure to hold the result
    @param time Time to format
    @ingroup MprTime
    @stability Stable
 */
PUBLIC void mprDecodeLocalTime(struct tm *timep, MprTime time);

/**
    Decode a time value into a tokenized UTC time structure.
    @description Safe replacement for gmtime. This call converts the supplied time value
        to UTC time and parses the result into a tm structure.
    @param timep Pointer to a tm structure to hold the result.
    @param time The time to format
    @ingroup MprTime
    @stability Stable
 */
PUBLIC void mprDecodeUniversalTime(struct tm *timep, MprTime time);

/**
    Convert a time value to local time and format as a string.
    @description Safe replacement for ctime.
    @param fmt Time format string. See #mprFormatUniversalTime for time formats.
    @param time Time to format. Use mprGetTime to retrieve the current time.
    @return The formatting time string
    @ingroup MprTime
    @stability Stable
 */
PUBLIC char *mprFormatLocalTime(cchar *fmt, MprTime time);

/**
    Convert a time value to universal time and format as a string.
    @description Format a time string. This uses strftime if available and so the supported formats vary from
        platform to platform. Strftime should supports some of these these formats described below.
    @param fmt Time format string
            \n
         %A ... full weekday name (Monday)
            \n
         %a ... abbreviated weekday name (Mon)
            \n
         %B ... full month name (January)
            \n
         %b ... abbreviated month name (Jan)
            \n
         %C ... century. Year / 100. (0-N)
            \n
         %c ... standard date and time representation
            \n
         %D ... date (%m/%d/%y)
            \n
         %d ... day-of-month (01-31)
            \n
         %e ... day-of-month with a leading space if only one digit ( 1-31)
            \n
         %F ... same as %Y-%m-%d
            \n
         %H ... hour (24 hour clock) (00-23)
            \n
         %h ... same as %b
            \n
         %I ... hour (12 hour clock) (01-12)
            \n
         %j ... day-of-year (001-366)
            \n
         %k ... hour (24 hour clock) (0-23)
            \n
         %l ... the hour (12-hour clock) as a decimal number (1-12); single digits are preceded by a blank.
            \n
         %M ... minute (00-59)
            \n
         %m ... month (01-12)
            \n
         %n ... a newline
            \n
         %P ... lower case am / pm
            \n
         %p ... AM / PM
            \n
         %R ... same as %H:%M
            \n
         %r ... same as %H:%M:%S %p
            \n
         %S ... second (00-59)
            \n
         %s ... seconds since epoch
            \n
         %T ... time (%H:%M:%S)
            \n
         %t ... a tab.
            \n
         %U ... week-of-year, first day sunday (00-53)
            \n
         %u ... the weekday (Monday as the first day of the week) as a decimal number (1-7).
            \n
         %v ... is equivalent to ``%e-%b-%Y''.
            \n
         %W ... week-of-year, first day monday (00-53)
            \n
         %w ... weekday (0-6, sunday is 0)
            \n
         %X ... standard time representation
            \n
         %x ... standard date representation
            \n
         %Y ... year with century
            \n
         %y ... year without century (00-99)
            \n
         %Z ... timezone name
            \n
         %z ... offset from UTC (-hhmm or +hhmm)
            \n
         %+ ... national representation of the date and time (the format is similar to that produced by date(1)).
            \n
         %% ... percent sign
            \n\n
            Some platforms may also support the following format extensions:
            \n
        %E* ... POSIX locale extensions. Where "*" is one of the characters: c, C, x, X, y, Y.
            \n
        %G ... a year as a decimal number with century. This year is the one that contains the greater part of
             the week (Monday as the first day of the week).
            \n
        %g ... the same year as in ``%G'', but as a decimal number without century (00-99).
            \n
        %O* ... POSIX locale extensions. Where "*" is one of the characters: d, e, H, I, m, M, S, u, U, V, w, W, y.
             Additionly %OB implemented to represent alternative months names (used standalone, without day mentioned).
            \n
        %V ... the week number of the year (Monday as the first day of the week) as a decimal number (01-53). If the week
             containing January 1 has four or more days in the new year, then it is week 1; otherwise it is the last
             week of the previous year, and the next week is week 1.
        \n\n
    Useful formats:
            \n
        RFC822: "%a, %d %b %Y %H:%M:%S %Z "Fri, 07 Jan 2003 12:12:21 PDT"
            \n
        "%T %F "12:12:21 2007-01-03"
            \n
        "%v "07-Jul-2003"
            \n
        RFC3399: "%FT%TZ" "1985-04-12T23:20:50.52Z" which is April 12 1985, 23:20.50 and 52 msec
\n\n
    @param time Time to format. Use mprGetTime to retrieve the current time.
    @return The formatting time string
    @ingroup MprTime
    @stability Stable
 */
PUBLIC char *mprFormatUniversalTime(cchar *fmt, MprTime time);

/**
    Format a time value as a local time.
    @description This call formats the time value supplied via \a timep.
    @param fmt The time format to use. See #mprFormatUniversalTime for time formats.
    @param timep The time value to format.
    @return The formatting time string.
    @ingroup MprTime
    @stability Stable
 */
PUBLIC char *mprFormatTm(cchar *fmt, struct tm *timep);

/**
    Get the system time.
    @description Get the system time in milliseconds. This is a monotonically increasing time counter.
        It does not represent wall-clock time.
    @return Returns the system time in milliseconds.
    @ingroup MprTime
    @stability Stable
 */
PUBLIC MprTicks mprGetTicks(void);

/**
    Get the time.
    @description Get the date/time in milliseconds since Jan 1 1970.
    @return Returns the time in milliseconds since Jan 1 1970.
    @ingroup MprTime
    @stability Stable
 */
PUBLIC MprTime mprGetTime(void);

/**
    Get a string representation of the current date/time
    @description Get the current date/time as a string according to the given format.
    @param fmt Date formatting string. See strftime for acceptable date format specifiers.
        If null, then this routine uses the #MPR_DEFAULT_DATE format.
    @return An allocated date string
    @ingroup MprTime
    @stability Stable
 */
PUBLIC char *mprGetDate(char *fmt);

/**
    Get the CPU tick count.
    @description Get the current CPU tick count. This is a system dependant high resolution timer. On some systems,
        this returns time in nanosecond resolution.
    @return Returns the CPU time in ticks. Will return the system time if CPU ticks are not available.
    @ingroup MprTicks
    @stability Internal
 */
PUBLIC uint64 mprGetHiResTicks(void);

#if (LINUX || MACOSX || WINDOWS) && (ME_CPU_ARCH == ME_CPU_X86 || ME_CPU_ARCH == ME_CPU_X64)
    #define MPR_HIGH_RES_TIMER 1
#else
    #define MPR_HIGH_RES_TIMER 0
#endif

#if ME_MPR_DEBUG_LOGGING
    #if MPR_HIGH_RES_TIMER
        #define MPR_MEASURE(level, tag1, tag2, op) \
            if ((level) <= MPR->logLevel) { \
                MprTicks elapsed, start = mprGetTicks(); \
                uint64  ticks = mprGetHiResTicks(); \
                op; \
                elapsed = mprGetTicks() - start; \
                if (elapsed < 1000) { \
                    mprLog("mpr time", level, "%s.%s elapsed %'lld msec, %'lld ticks", \
                        tag1, tag2, elapsed, mprGetHiResTicks() - ticks); \
                } else { \
                    mprLog("mpr time", level, "%s.%s elapsed %'lld msec", tag1, tag2, elapsed); \
                } \
            } else { \
                op; \
            }
    #else
        #define MPR_MEASURE(level, tag1, tag2, op) \
            if ((level) <= MPR->logLevel) { \
                MprTicks start = mprGetTicks(); \
                op; \
                mprLog("mpr time", level, "%s.%s elapsed %'lld msec", tag1, tag2, mprGetTicks() - start); \
            } else { \
                op; \
            }
    #endif
#else
    #define MPR_MEASURE(level, tag1, tag2, op) op
#endif

/**
    Return the time remaining until a timeout has elapsed
    @param mark Starting time stamp
    @param timeout Time in milliseconds
    @return Time in milliseconds until the timeout elapses
    @ingroup MprTime
    @stability Stable
 */
PUBLIC MprTicks mprGetRemainingTicks(MprTicks mark, MprTicks timeout);

/**
    Get the elapsed time since a ticks mark. Create the ticks mark with mprGetTicks()
    @param mark Starting time stamp
    @returns the time elapsed since the mark was taken.
    @ingroup MprTime
    @stability Stable
 */
PUBLIC MprTicks mprGetElapsedTicks(MprTicks mark);

/**
    Get the elapsed time since a starting time mark.
    @param mark Starting time created via mprGetTime()
    @returns the time elapsed since the mark was taken.
    @ingroup MprTime
    @stability Stable
 */
PUBLIC MprTime mprGetElapsedTime(MprTime mark);

/*
    Convert a time structure into a time value using local time.
    @param timep Pointer to a time structure
    @return a time value
    @ingroup MprTime
    @stability Stable
 */
PUBLIC MprTime mprMakeTime(struct tm *timep);

/*
    Convert a time structure into a time value using UTC time.
    @param timep Pointer to a time structure
    @return a time value
    @ingroup MprTime
    @stability Stable
 */
PUBLIC MprTime mprMakeUniversalTime(struct tm *tm);

/**
    Constants for mprParseTime
 */
#define MPR_LOCAL_TIMEZONE      MAXINT      /**< Use local timezone */
#define MPR_UTC_TIMEZONE        0           /**< Use UTC timezone */

/*
    Parse a string into a time value
    @description Try to intelligently parse a date.
    This is a tolerant parser. It is not validating and will do its best to parse any possible date string.
    Supports the following date/time formats:
    \n\n
        ISO dates: 2009-05-21t16:06:05.000z
    \n\n
        Date:  07/28/2014, 07/28/08, Jan/28/2014, Jaunuary-28-2014, 28-jan-2014.
    \n\n
        Support date order: dd/mm/yy, mm/dd/yy and yyyy/mm/dd
    \n\n
        Support separators "/", ".", "-"
    \n\n
        Timezones: GMT|UTC[+-]NN[:]NN
    \n\n
        Time: 10:52[:23]
    \n\n
    @param time Pointer to a time value to receive the parsed time value
    @param dateString String to parse
    @param timezone Timezone in which to interpret the date
    @param defaults Date default values to use for missing components
    @returns Zero if successful
    @ingroup MprTime
    @stability Stable

 */
PUBLIC int mprParseTime(MprTime *time, cchar *dateString, int timezone, struct tm *defaults);

/**
    Get the current timezone offset for a given time
    @description Calculate the current timezone (including DST)
    @param when Time to examine to extract the timezone
    @returns Returns a timezone offset in msec.  Local time == (UTC + offset).
    @ingroup MprTime
    @stability Stable
 */
PUBLIC int mprGetTimeZoneOffset(MprTime when);

/*********************************** Lists ************************************/
/*
    List flags
 */
#define MPR_OBJ_LIST            0x1     /**< Object is a hash */
#define MPR_LIST_STATIC_VALUES  0x20    /**< Flag for #mprCreateList when values are permanent */
#define MPR_LIST_STABLE         0x40    /**< Contents are stable or only accessed by one thread. Does not need thread locking */

/**
    List data structure.
    @description The MprList is a dynamic, growable list suitable for storing pointers to arbitrary objects.
    @see MprList MprListCompareProc mprAddItem mprAddNullItem mprAppendList mprClearList mprCloneList mprCopyList
        mprCreateKeyPair mprCreateList mprGetFirstItem mprGetItem mprGetLastItem mprGetListCapacity mprGetListLength
        mprGetNextItem mprGetPrevItem mprInitList mprInsertItemAtPos mprLookupItem mprLookupStringItem mprPopItem
        mprPushItem mprRemoveItem mprRemoveItemAtPos mprRemoveRangeOfItems mprRemoveStringItem mprSetItem
        mprSetListLimits mprSortList
    @defgroup MprList MprList
    @stability Internal.
 */
typedef struct MprList {
    int         flags;                  /**< Control flags */
    int         size;                   /**< Current list capacity */
    int         length;                 /**< Current length of the list contents */
    int         maxSize;                /**< Maximum capacity */
    MprMutex    *mutex;                 /**< Multithread lock */
    void        **items;                /**< List item data */
} MprList;

/**
    List comparison procedure for sorting
    @description Callback function signature used by #mprSortList
    @param arg1 First list item to compare
    @param arg2 Second list item to compare
    @returns Return zero if the items are equal. Return -1 if the first arg is less than the second. Otherwise return 1.
    @ingroup MprList
    @stability Stable.
 */
typedef int (*MprListCompareProc)(cvoid *arg1, cvoid *arg2);

/**
    Add an item to a list
    @description Add the specified item to the list. The list must have been previously created via
        mprCreateList. The list will grow as required to store the item
    @param list List pointer returned from #mprCreateList
    @param item Pointer to item to store
    @return Returns a positive list index for the inserted item. If the item cannot be inserted due
        to a memory allocation failure, -1 is returned
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprAddItem(MprList *list, cvoid *item);

/**
    Add a null item to the list.
    @description Add a null item to the list. This item does not count in the length returned by #mprGetListLength
    and will not be visible when iterating using #mprGetNextItem.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprAddNullItem(MprList *list);

/**
    Append a list
    @description Append the contents of one list to another. The list will grow as required to store the item
    @param list List pointer returned from #mprCreateList
    @param add List whose contents are added
    @return Returns a pointer to the original list if successful. Returns NULL on memory allocation errors.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC MprList *mprAppendList(MprList *list, MprList *add);

/**
    Clears the list of all items.
    @description Resets the list length to zero and clears all items.
    @param list List pointer returned from mprCreateList.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC void mprClearList(MprList *list);

/**
    Clone a list and all elements
    @description Copy the contents of a list into a new list.
    @param src Source list to copy
    @return Returns a new list reference
    @ingroup MprList
    @stability Stable.
 */
PUBLIC MprList *mprCloneList(MprList *src);

/**
    Copy list contents
    @description Copy the contents of a list into an existing list. The destination list is cleared first and
        has its dimensions set to that of the source clist.
    @param dest Destination list for the copy
    @param src Source list
    @return Returns zero if successful, otherwise a negative MPR error code.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprCopyListContents(MprList *dest, MprList *src);

/**
    Create a list.
    @description Creates an empty list. MprList's can store generic pointers. They automatically grow as
        required when items are added to the list.
    @param size Initial capacity of the list. Set to < 0 to get a growable list with a default initial size.
        Set to 0 to to create the list but without any initial list storage. Then call mprSetListLimits to define
        the initial and maximum list size.
    @param flags Control flags. Possible values are: MPR_LIST_STATIC_VALUES to indicate list items are static
        and should not be marked for GC. MPR_LIST_STABLE to create an optimized list for private use that is not thread-safe.
    @return Returns a pointer to the list.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC MprList *mprCreateList(int size, int flags);

/**
    Create a list of words
    @description Create a list of words from the given string. The word separators are white space and comma.
    @param str String containing white space or comma separated words
    @return Returns a list of words
    @ingroup MprList
    @stability Stable
 */
PUBLIC MprList *mprCreateListFromWords(cchar *str);

/**
    Get the first item in the list.
    @description Returns the value of the first item in the list. After calling this routine, the remaining
        list items can be walked using mprGetNextItem.
    @param list List pointer returned from mprCreateList.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC void *mprGetFirstItem(MprList *list);

#if DOXYGEN || 1
/**
    Get an list item.
    @description Get an list item specified by its index.
    @param list List pointer returned from mprCreateList.
    @param index Item index into the list. Indexes have a range from zero to the lenghth of the list - 1.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC void *mprGetItem(MprList *list, int index);
#else
    #define mprGetItem(lp, index) (index < 0 || index >= lp->length) ? 0 : lp->items[index];
#endif

/**
    Get the last item in the list.
    @description Returns the value of the last item in the list. After calling this routine, the remaining
        list items can be walked using mprGetPrevItem.
    @param list List pointer returned from mprCreateList.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC void *mprGetLastItem(MprList *list);

/**
    Get the current capacity of the list.
    @description Returns the capacity of the list. This will always be equal to or greater than the list length.
    @param list List pointer returned from mprCreateList.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprGetListCapacity(MprList *list);

/**
    Get the number of items in the list.
    @description Returns the number of items in the list. This will always be less than or equal to the list capacity.
    @param list List pointer returned from mprCreateList.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprGetListLength(MprList *list);

/**
    Get the next item in the list.
    @description Returns the value of the next item in the list. Before calling
        this routine, mprGetFirstItem must be called to initialize the traversal of the list.
    @param list List pointer returned from mprCreateList.
    @param lastIndex Pointer to an integer that will hold the last index retrieved.
    @return Next item in list or null for an empty list or after the last item.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC void *mprGetNextItem(MprList *list, int *lastIndex);

/**
    Get the next item in a stable list.
    This is an optimized version of mprGetNextItem.
    @description Returns the value of the next item in the list. Before calling
        this routine, mprGetFirstItem must be called to initialize the traversal of the list.
    @param list List pointer returned from mprCreateList.
    @param lastIndex Pointer to an integer that will hold the last index retrieved.
    @return Next item in list
    @ingroup MprList
    @internal
    @stability Stable
 */
PUBLIC void *mprGetNextStableItem(MprList *list, int *lastIndex);

/**
    Get the previous item in the list.
    @description Returns the value of the previous item in the list. Before
        calling this routine, mprGetFirstItem and/or mprGetNextItem must be
        called to initialize the traversal of the list.
    @param list List pointer returned from mprCreateList.
    @param lastIndex Pointer to an integer that will hold the last index retrieved.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC void *mprGetPrevItem(MprList *list, int *lastIndex);

/**
    Initialize a list structure
    @description If a list is statically declared inside another structure, mprInitList can be used to
        initialize it before use.
    @param list Reference to the MprList struct.
    @param flags Control flags. Possible values are: MPR_LIST_STATIC_VALUES to indicate list items are static
        and should not be marked for GC.  MPR_LIST_STABLE to create an optimized list for private use that is not
        thread-safe.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC void mprInitList(MprList *list, int flags);

/**
    Insert an item into a list at a specific position
    @description Insert the item into the list before the specified position. The list will grow as required
        to store the item
    @param list List pointer returned from #mprCreateList
    @param index Location at which to store the item. The previous item at this index is moved up to make room.
    @param item Pointer to item to store
    @return Returns the position index (positive integer) if successful. If the item cannot be inserted due
        to a memory allocation failure, -1 is returned
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprInsertItemAtPos(MprList *list, int index, cvoid *item);

/**
    Convert a list of strings to a single string. This uses the specified join string between the elements.
    @param list List pointer returned from mprCreateList.
    @param join String to use as the element join string. May be null.
    @ingroup MprList
    @stability Stable
 */
PUBLIC char *mprListToString(MprList *list, cchar *join);

/**
    Find an item and return its index.
    @description Search for an item in the list and return its index.
    @param list List pointer returned from mprCreateList.
    @param item Pointer to value stored in the list.
    @return Positive list index if found, otherwise a negative MPR error code.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprLookupItem(MprList *list, cvoid *item);

/**
    Find a string item and return its index.
    @description Search for the first matching string in the list and return its index.
    @param list List pointer returned from mprCreateList.
    @param str Pointer to string to look for.
    @return Positive list index if found, otherwise a negative MPR error code.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprLookupStringItem(MprList *list, cchar *str);

/**
    Remove an item from the list
    @description Search for a specified item and then remove it from the list.
    @param list List pointer returned from mprCreateList.
    @param item Item pointer to remove.
    @return Returns the positive index of the removed item, otherwise a negative MPR error code.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprRemoveItem(MprList *list, cvoid *item);

/**
    Remove an item from the list
    @description Removes the element specified by \a index, from the list. The
        list index is provided by mprInsertItem.
    @return Returns the positive index of the removed item, otherwise a negative MPR error code.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprRemoveItemAtPos(MprList *list, int index);

/**
    Remove the last item from the list
    @description Remove the item at the highest index position.
    @param list List pointer returned from mprCreateList.
    @return Returns the positive index of the removed item, otherwise a negative MPR error code.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprRemoveLastItem(MprList *list);

/**
    Remove a range of items from the list.
    @description Remove a range of items from the list. The range is specified
        from the \a start index up to and including the \a end index.
    @param list List pointer returned from mprCreateList.
    @param start Starting item index to remove (inclusive)
    @param end Ending item index to remove (inclusive)
    @return Returns zero if successful, otherwise a negative MPR error code.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprRemoveRangeOfItems(MprList *list, int start, int end);

/**
    Remove a string item from the list
    @description Search for the first matching string and then remove it from the list.
    @param list List pointer returned from mprCreateList.
    @param str String value to remove.
    @return Returns the positive index of the removed item, otherwise a negative MPR error code.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprRemoveStringItem(MprList *list, cchar *str);

/**
    Set a list item
    @description Update the list item stored at the specified index
    @param list List pointer returned from mprCreateList.
    @param index Location to update
    @param item Pointer to item to store
    @return Returns the old item previously at that location index
    @ingroup MprList
    @stability Stable.
 */
PUBLIC void *mprSetItem(MprList *list, int index, cvoid *item);

/**
    Define the list size limits
    @description Define the list initial size and maximum size it can grow to.
    @param list List pointer returned from mprCreateList.
    @param initialSize Initial size for the list. This call will allocate space for at least this number of items.
    @param maxSize Set the maximum limit the list can grow to become.
    @return Returns zero if successful, otherwise a negative MPR error code.
    @ingroup MprList
    @stability Stable.
 */
PUBLIC int mprSetListLimits(MprList *list, int initialSize, int maxSize);

/**
    Quicksort callback function
    @description This is a quicksort callback with a context argument.
    @param p1 Pointer to first element
    @param p2 Pointer to second element
    @param ctx Context argument to provide to comparison function
    @return -1, 0, or 1, depending on if the elements are p1 < p2, p1 == p2 or p1 > p2
    @ingroup MprList
    @stability Stable
 */
typedef int (*MprSortProc)(cvoid *p1, cvoid *p2, void *ctx);

/**
    Quicksort
    @description This is a quicksort with a context argument.
    @param base Base of array to sort
    @param num Number of array elements
    @param width Width of array elements
    @param compare Comparison function
    @param ctx Context argument to provide to comparison function
    @return The base array for chaining
    @ingroup MprList
    @stability Stable
 */
PUBLIC void *mprSort(void *base, ssize num, ssize width, MprSortProc compare, void *ctx);

/**
    Sort a list
    @description Sort a list using the sort ordering dictated by the supplied compare function.
    @param list List pointer returned from mprCreateList.
    @param compare Comparison function. If null, then a default string comparison is used.
    @param ctx Context to provide to comparison function
    @return The sorted list
    @ingroup MprList
    @stability Stable
 */
PUBLIC MprList *mprSortList(MprList *list, MprSortProc compare, void *ctx);

/**
    Key value pairs for use with MprList or MprKey
    @ingroup MprList
    @stability Stable
 */
typedef struct MprKeyValue {
    void        *key;               /**< Key string (managed) */
    void        *value;             /**< Associated value for the key (managed) */
    int         flags;              /**< General flags word */
} MprKeyValue;

/**
    Create a key / value pair
    @description Allocate and initialize a key value pair for use by the MprList or MprHash modules.
    @param key Key string
    @param value Key value string
    @param flags Flags value
    @returns An initialized MprKeyValue
    @ingroup MprList
    @stability Stable
 */
PUBLIC MprKeyValue *mprCreateKeyPair(cchar *key, cchar *value, int flags);

/**
    Pop an item
    @description Treat the list as a stack and pop the last pushed item
    @param list List pointer returned from mprCreateList.
    @return Returns the last pushed item. If the list is empty, returns NULL.
    @ingroup MprList
    @stability Stable
  */
PUBLIC void *mprPopItem(MprList *list);

/**
    Push an item onto the list
    @description Treat the list as a stack and push the last pushed item
    @param list List pointer returned from mprCreateList.
    @param item Item to push onto the list
    @return Returns a positive integer list index for the inserted item. If the item cannot be inserted due
        to a memory allocation failure, -1 is returned
    @ingroup MprList
    @stability Stable
  */
PUBLIC int mprPushItem(MprList *list, cvoid *item);

#define MPR_GET_ITEM(list, index) list->items[index]
#define ITERATE_ITEMS(list, item, next) next = 0; (item = mprGetNextItem(list, &next)) != 0;
#define ITERATE_STABLE_ITEMS(list, item, next) next = 0; (item = mprGetNextStableItem(list, &next)) != 0;
#define mprGetListLength(lp) ((lp) ? (lp)->length : 0)

/********************************** Logging ***********************************/
/**
    Logging Services
    @defgroup MprLog MprLog
    @see MprLogHandler mprAssert mprError mprGetLogFile mprGetLogHandler mprInfo mprLog mprRawLog mprDebug
        mprSetLogFile mprSetLogHandler mprSetLogLevel mprStaticError mprUsingDefaultLogHandler mprWarn
    @stability Internal
 */
typedef struct MprLog { int dummy; } MprLog;

/**
    Log handler callback type.
    @description Callback prototype for the log handler. Used by mprSetLogHandler to define
        a message logging handler to process log and error messages. See #mprLog for more details.
    @param file Source filename. Derived by using __FILE__.
    @param line Source line number. Derived by using __LINE__.
    @param flags Error flags.
    @param tags List of space separated tag words.
    @param level Message logging level. Levels are 0-5 with five being the most verbose.
    @param msg Message being logged.
    @ingroup MprLog
    @stability Stable
 */
typedef void (*MprLogHandler)(cchar *tags, int level, cchar *msg);

/**
    Output an assure assertion failed message.
    @description This will emit an assure assertion failed message to the standard error output.
        It may bypass the logging system.
    @param loc Source code location string. Use MPR_LOC to define a file name and line number string suitable for this
        parameter.
    @param msg Simple string message to output
    @ingroup MprLog
    @stability Stable
 */
PUBLIC void mprAssert(cchar *loc, cchar *msg);

/**
    Initialize the log service
    @ingroup MprLog
    @stability Internal
 */
PUBLIC void mprCreateLogService(void);

/**
    Backup a log
    @param path Base log filename
    @param count Count of archived logs to keep
    @ingroup MprLog
    @stability Stable
 */
PUBLIC int mprBackupLog(cchar *path, int count);

/**
    Default MPR log handler
    @param tags Descriptive tag words to classify this message.
    @param level Logging level for this message. The level is 0-5 with five being the most verbose.
    @param msg Message to log
    @ingroup MprLog
    @stability Stable
 */
PUBLIC void mprDefaultLogHandler(cchar *tags, int level, cchar *msg);

/**
    Log an error message.
    @description Send an error message to the MPR debug logging subsystem. The
        message will be to the log handler defined by #mprSetLogHandler. It
        is up to the log handler to respond appropriately and log the message.
        This will invoke mprLog with a severity tag of "error".
    @param fmt Printf style format string. Variable number of arguments to
    @param ... Variable number of arguments for printf data
    @ingroup MprLog
    @stability Stable
 */
PUBLIC void mprError(cchar *fmt, ...) PRINTF_ATTRIBUTE(1,2);

/**
    Get the log file object
    @description Returns the MprFile object used for logging
    @returns An MprFile object for logging
    @ingroup MprLog
    @stability Stable
 */
PUBLIC struct MprFile *mprGetLogFile(void);

/**
    Get the current MPR debug log handler.
    @description Get the log handler defined via #mprSetLogHandler
    @returns A function of the signature #MprLogHandler
    @ingroup MprLog
    @stability Stable
 */
PUBLIC MprLogHandler mprGetLogHandler(void);

#if DOXYGEN
/**
    Write a message to the error log file.
    @description Send a message to the MPR error logging subsystem.
        The purpose of the error log is to record essential configuration and error conditions. Per-request trace
        typically is sent to a separate trace log.
        \n\n
        By default, error log messages are sent to the standard error output.
        Applications may redirect output by installing a log handler using #mprSetLogHandler.
        \n\n
        Log messages should be a single text line to facilitate machine processing of log files. Descriptive tag words
        may be provided to indicate a severity level and to classifiy messages.
        By convention, tags may include one of the severity levels defined in RFC 5424: "debug",
        "info", "notice", "warn", "error", "critical". Messages using the "error", "critical" tags should use
        a level of zero.  Tags should be space separated.
        \n\n
        Logging typically is enabled in both debug and release builds and may be controlled via the build define
        ME_MPR_LOGGING which is typically set via the MakeMe setting "logging: true".
        \n\n
        The #mprDebug API may be used to emit log messages only in debug builds.
        \n\n
        If level zero is used, the message is also sent to any relevant operating system logging facility such as
        syslog or the Windows event database.
        \n\n
        It is good practice to only include debug trace at levels above level 2 so that essential error messages are clearly
        visible in the error log and are not swamped by debug messages.
    @param tags Descriptive space separated tag words to classify this message.
        The default log handler emits messages in three formats depending on whether MPR_LOG_DETAILED is provided to
        #mprStartLogging and the value of the tags parameter.
        If MPR_LOG_DETAILED and tags are supplied, the format is: "MM/DD/YY HH:MM:SS LEVEL TAGS, Message". Otherwise a
        a simplified output format is used: "Name: severity: message", where severity is set to "error" for level 0
        messages. This is useful for utility programs.
        If tags are null, the message is output raw, without any any prefixes.
    @param level Logging level for this message. The level is 0-5 with five being the most verbose.
    @param fmt Printf style format string. Variable number of arguments to print
    @param ... Variable number of arguments for printf data
    @remarks mprLog is highly useful as a debugging aid.
    @ingroup MprLog
    @stability Stable
 */
PUBLIC void mprLog(cchar *tags, int level, cchar *fmt, ...);
#endif
PUBLIC void mprLogProc(cchar *tags, int level, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Show the product configuration at the start of the log file
    @ingroup MprLog
    @stability Stable
 */
PUBLIC void mprLogConfig(void);

/**
    Set the log rotation parameters
    @param logSize If the size is zero, then the log file will be rotated on each application boot. Otherwise,
        the log file will be rotated if on application boot, the log file is larger than this size.
    @param backupCount Count of the number of log files to keep
    @param flags Set to MPR_LOG_ANEW to truncate existing files (after backup).
    @ingroup MprLog
    @stability Stable
 */
PUBLIC void mprSetLogBackup(ssize logSize, int backupCount, int flags);

/**
    Set a file to be used for logging
    @param file MprFile object instance
    @stability Stable
 */
PUBLIC void mprSetLogFile(struct MprFile *file);

/**
    Set an MPR debug log handler.
    @description Defines a callback handler for MPR debug and error log messages. When output is sent to
        the debug channel, the log handler will be invoked to accept the output message.
    @param handler Callback handler
    @return Prior log handler
    @stability Stable
 */
PUBLIC MprLogHandler mprSetLogHandler(MprLogHandler handler);

/**
    Start logging
    @param logSpec Set the log file name and level. The format is "pathName[:level]".
    The level is a verbosity level from 0 to 5 with 5 being the most verbose.
    The following levels are generally observed:
    <ul>
        <li>0 - Essential messages: errors and warnings</li>
        <li>1 - Non-essential warnings</li>
        <li>2 - Configuration information</li>
        <li>3 - Useful informational messages</li>
        <li>4 - Debug information</li>
        <li>5 - Most verbose levels of messages useful for debugging</li>
    </ul>
    If logSpec is set to null, then logging is not started.
    The filename may be set to "stdout", "stderr" or "none". The latter is the same as supplying null as the logSpec.
    @param flags Set to MPR_LOG_CONFIG to show the configuration in the log file. Set to MPR_LOG_CMDLINE if a command line
        override has been used to initiate logging. Set MPR_LOG_DETAILED to use the detailed message format.
        Set MPR_LOG_ANEW to truncate existing log files after backup.
    @return Zero if successful, otherwise a negative Mpr error code. See the log for diagnostics.
    @ingroup MprLog
    @stability Stable
*/
PUBLIC int mprStartLogging(cchar *logSpec, int flags);

#if DOXYGEN
/**
    Write a log message to the log file when the product is built in debug mode.
    @description This routine permits the addition of debug messages that are compiled out in production builds.
    @param tags List of space separated tag words.
    @param level Logging level for this message. The level is 0-5 with five being the most verbose.
    @param fmt Printf style format string. Variable number of arguments to
    @param ... Variable number of arguments for printf data
    @ingroup MprLog
    @stability Stable
 */
PUBLIC void mprDebug(cchar *tags, int level, cchar *fmt, ...);
#endif
PUBLIC void mprLogProc(cchar *tags, int level, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Determine if the app is using the default MPR log handler.
    @description Returns true if no custom log handler has been installed.
    @returns True if using the default log handler
    @ingroup MprLog
    @stability Stable
 */
PUBLIC int mprUsingDefaultLogHandler(void);

#if ME_MPR_DEBUG_LOGGING
    #define mprDebug(tags, l, ...) if ((l) <= MPR->logLevel) { mprLogProc(tags, l, __VA_ARGS__); } else {}
#else
    #define mprDebug(tags, l, ...) if (1) ; else {}
#endif

#if ME_MPR_LOGGING
    #define mprLog(tags, l, ...) if ((l) <= MPR->logLevel) { mprLogProc(tags, l, __VA_ARGS__); } else {}
#else
    #define mprLog(tags, l, ...) if (1) ; else {}
#endif

/************************************ Hash ************************************/
/**
    Hash table entry structure.
    @description The hash structure supports growable hash tables with high performance, collision resistant hashes.
    Each hash entry has a descriptor entry. This is used to manage the hash table link chains.
    @see MprKey MprHashProc MprHash mprAddDuplicateHash mprAddKey mprAddKeyFmt mprCloneHash mprCreateHash
        mprGetFirstKey mprGetHashLength mprGetKeyBits mprGetNextKey mprLookupKey mprLookupKeyEntry mprRemoveKey
        mprSetKeyBits mprBlendHash
    @defgroup MprHash MprHash
    @stability Internal.
 */
typedef struct MprKey {
    struct MprKey   *next;              /**< Next symbol in hash chain */
    char            *key;               /**< Hash key */
    cvoid           *data;              /**< Pointer to symbol data (managed) */
    int             type: 4;            /**< Data type */
    int             bucket: 28;         /**< Hash bucket index */
} MprKey;

/**
    Hashing function to use for the table
    @param name Name to hash
    @param len Length of the name to hash
    @return An integer hash index
    @internal
    @stability Internal.
*/
typedef uint (*MprHashProc)(cvoid *name, ssize len);

/*
    Flags for MprHash
 */
#define MPR_OBJ_HASH            0x1     /**< Object is a hash */
#define MPR_HASH_CASELESS       0x10    /**< Key comparisons ignore case */
#define MPR_HASH_UNICODE        0x20    /**< Hash keys are unicode strings */
#define MPR_HASH_STATIC_KEYS    0x40    /**< Keys are permanent - don't dup or mark */
#define MPR_HASH_STATIC_VALUES  0x80    /**< Values are permanent - don't mark */
#define MPR_HASH_MANAGED_KEYS   0x100   /**< Keys are managed - mark but don't dup */
#define MPR_HASH_MANAGED_VALUES 0x200   /**< Values are managed - mark but don't dup */
#define MPR_HASH_UNIQUE         0x400   /**< Add to existing will fail */
#define MPR_HASH_STABLE         0x800   /**< Contents are stable or only accessed by one thread. Does not need thread locking */
#define MPR_HASH_STATIC_ALL     (MPR_HASH_STATIC_KEYS | MPR_HASH_STATIC_VALUES)

/**
    Hash table control structure
    @see MprHash
    @stability Internal.
 */
typedef struct MprHash {
    int             flags;              /**< Hash control flags */
    int             size;               /**< Size of the buckets array */
    int             length;             /**< Number of symbols in the table */
    MprKey          **buckets;          /**< Hash collision bucket table */
    MprHashProc     fn;                 /**< Hash function */
    MprMutex        *mutex;             /**< GC marker sync */
} MprHash;

/*
    Macros
 */
#define ITERATE_KEYS(table, key) key = 0; (key = mprGetNextKey(table, key)) != 0;
#define ITERATE_KEY_DATA(table, key, item) key = 0; (key = mprGetNextKey(table, key)) != 0 && ((item = (void*) ((key)->data)) != 0 || 1);

/**
    Add a duplicate symbol value into the hash table
    @description Add a symbol to the hash which may clash with an existing entry. Duplicate symbols can be added to
        the hash, but only one may be retrieved via #mprLookupKey. To recover duplicate entries walk the hash using
        #mprGetNextKey.
    @param table Symbol table returned via mprCreateHash.
    @param key String key of the symbol entry to delete.
    @param ptr Arbitrary pointer to associate with the key in the table.
    @return Integer count of the number of entries.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprKey *mprAddDuplicateKey(MprHash *table, cvoid *key, cvoid *ptr);

/**
    Add a symbol value into the hash table
    @description Associate an arbitrary value with a string symbol key and insert into the symbol table.
        This will replace existing key values. Use mprAddDuplicateKey to allow duplicates.
    @param table Symbol table returned via mprCreateHash.
    @param key String key of the symbol entry to delete.
    @param ptr Arbitrary pointer to associate with the key in the table.
    @return Added MprKey reference.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprKey *mprAddKey(MprHash *table, cvoid *key, cvoid *ptr);

/**
    Add a symbol value into the hash table and set the key type.
    @description Associate an arbitrary value with a string symbol key and insert into the symbol table.
    @param table Symbol table returned via mprCreateHash.
    @param key String key of the symbol entry to delete.
    @param ptr Arbitrary pointer to associate with the key in the table.
    @param type Type of value.
    @return Added MprKey reference.
    @ingroup MprHash
    @stability internal.
 */
PUBLIC MprKey *mprAddKeyWithType(MprHash *table, cvoid *key, cvoid *ptr, int type);

/**
    Add a key with a formatting value into the hash table
    @description Associate a formatted value with a key and insert into the symbol table.
    @param table Symbol table returned via mprCreateHash.
    @param key String key of the symbol entry to delete.
    @param fmt Format string. See #mprPrintf.
    @return Integer count of the number of entries.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprKey *mprAddKeyFmt(MprHash *table, cvoid *key, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Copy a hash table
    @description Create a new hash table and copy all the entries from an existing table.
    @param table Symbol table returned via mprCreateHash.
    @return A new hash table initialized with the contents of the original hash table.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprHash *mprCloneHash(MprHash *table);

/**
    Create a hash table
    @description Creates a hash table that can store arbitrary objects associated with string key values.
    @param hashSize Size of the hash table for the symbol table. Should be a prime number. Set to 0 or -1 to get
        a default (small) hash table.
    @param flags Table control flags. Use MPR_HASH_CASELESS for case insensitive comparisions, MPR_HASH_UNICODE
        if the hash keys are unicode strings, MPR_HASH_STATIC_KEYS if the keys are permanent and should not be
        managed for Garbage collection, and MPR_HASH_STATIC_VALUES if the values are permanent.
        MPR_HASH_STABLE to create an optimized list when the contents are stable or only accessed by one thread.
    @return Returns a pointer to the allocated symbol table.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprHash *mprCreateHash(int hashSize, int flags);

/**
    Create a hash of words
    @description Create a hash table of words from the given string. The hash key entry is the same as the key.
        The word separators are white space and comma.
    @param str String containing white space or comma separated words
    @return Returns a hash of words
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprHash *mprCreateHashFromWords(cchar *str);

/**
    Return the first symbol in a symbol entry
    @description Prepares for walking the contents of a symbol table by returning the first entry in the symbol table.
    @param table Symbol table returned via mprCreateHash.
    @return Pointer to the first entry in the symbol table.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprKey *mprGetFirstKey(MprHash *table);

/**
    Return the next symbol in a symbol entry
    @description Continues walking the contents of a symbol table by returning
        the next entry in the symbol table. A previous call to mprGetFirstSymbol
        or mprGetNextSymbol is required to supply the value of the \a last
        argument.
    @param table Symbol table returned via mprCreateHash.
    @param last Symbol table entry returned via mprGetFirstSymbol or mprGetNextSymbol.
    @return Pointer to the first entry in the symbol table.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprKey *mprGetNextKey(MprHash *table, MprKey *last);

/**
    Return the count of symbols in a symbol entry
    @description Returns the number of symbols currently existing in a symbol table.
    @param table Symbol table returned via mprCreateHash.
    @return Integer count of the number of entries.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC int mprGetHashLength(MprHash *table);

/**
    Lookup a symbol in the hash table.
    @description Lookup a symbol key and return the value associated with that key.
    @param table Symbol table returned via mprCreateHash.
    @param key String key of the symbol entry to delete.
    @return Value associated with the key when the entry was inserted via mprInsertSymbol.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC void *mprLookupKey(MprHash *table, cvoid *key);

/**
    Lookup a symbol in the hash table and return the hash entry
    @description Lookup a symbol key and return the hash table descriptor associated with that key.
    @param table Symbol table returned via mprCreateHash.
    @param key String key of the symbol entry to delete.
    @return MprKey for the entry
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprKey *mprLookupKeyEntry(MprHash *table, cvoid *key);

/**
    Remove a symbol entry from the hash table.
    @description Removes a symbol entry from the symbol table. The entry is looked up via the supplied \a key.
    @param table Symbol table returned via mprCreateHash.
    @param key String key of the symbol entry to delete.
    @return Returns zero if successful, otherwise a negative MPR error code is returned.
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC int mprRemoveKey(MprHash *table, cvoid *key);

/**
    Blend two hash tables
    @description Blend a hash table into a target hash
    @param target Target hash to receive the properties from the other hash
    @param other Hash to provide properties to blend
    @return Returns target
    @ingroup MprHash
    @stability Stable.
 */
PUBLIC MprHash *mprBlendHash(MprHash *target, MprHash *other);

/**
    Convert a hash of strings to a single string
    @param hash Hash pointer returned from mprCreateHash.
    @param join String to use as the element join string.
    @return String consisting of the joined hash values
    @ingroup MprHash
    @stability Stable
*/
PUBLIC char *mprHashToString(MprHash *hash, cchar *join);

/**
    Convert hash keys to a single string
    @param hash Hash pointer returned from mprCreateHash.
    @param join String to use as the element join string.
    @return String consisting of the joined hash keys
    @ingroup MprHash
    @stability Stable
*/
PUBLIC char *mprHashKeysToString(MprHash *hash, cchar *join);

/*********************************** Files ************************************/
/**
    Signed file offset data type. Supports large files greater than 4GB in size on all systems.
 */
typedef Offset MprOff;

#ifndef ME_MPR_DISK
    #define ME_MPR_DISK 1
#endif
#ifndef ME_MPR_ROM_MOUNT
    #define ME_MPR_ROM_MOUNT "/rom"
#endif

/*
    Prototypes for file system switch methods
    All internal.
 */
typedef bool    (*MprAccessFileProc)(struct MprFileSystem *fs, cchar *path, int omode);
typedef int     (*MprDeleteFileProc)(struct MprFileSystem *fs, cchar *path);
typedef int     (*MprDeleteDirProc)(struct MprFileSystem *fs, cchar *path);
typedef int     (*MprGetPathInfoProc)(struct MprFileSystem *fs, cchar *path, struct MprPath *info);
typedef char   *(*MprGetPathLinkProc)(struct MprFileSystem *fs, cchar *path);
typedef MprList*(*MprListDirProc)(struct MprFileSystem *fs, cchar *path);
typedef int     (*MprMakeDirProc)(struct MprFileSystem *fs, cchar *path, int perms, int owner, int group);
typedef int     (*MprMakeLinkProc)(struct MprFileSystem *fs, cchar *path, cchar *target, int hard);
typedef int     (*MprCloseFileProc)(struct MprFile *file);
typedef ssize   (*MprReadFileProc)(struct MprFile *file, void *buf, ssize size);
typedef MprOff  (*MprSeekFileProc)(struct MprFile *file, int seekType, MprOff distance);
typedef int     (*MprSetBufferedProc)(struct MprFile *file, ssize initialSize, ssize maxSize);
typedef int     (*MprTruncateFileProc)(struct MprFileSystem *fs, cchar *path, MprOff size);
typedef ssize   (*MprWriteFileProc)(struct MprFile *file, cvoid *buf, ssize count);

#if !DOXYGEN
/* Work around doxygen bug */
typedef struct MprFile* (*MprOpenFileProc)(struct MprFileSystem *fs, cchar *path, int omode, int perms);
#endif

/**
    File system service
    @description The MPR provides a file system abstraction to support non-disk based file access such as flash or
        other ROM based file systems. The MprFileSystem structure defines a virtual file system interface that
        will be invoked by the various MPR file routines.
    @see MprRomInode mprAddFileSystem mprCreateDiskFileSystem mprCreateFileSystem mprCreateRomFileSystem
        mprLookupFileSystem mprSetPathNewline mprSetPathSeparators
    @defgroup MprFileSystem MprFileSystem
    @stability Internal
 */
typedef struct MprFileSystem {
    MprAccessFileProc   accessPath;     /**< Virtual access file routine */
    MprDeleteFileProc   deletePath;     /**< Virtual delete file routine */
    MprGetPathInfoProc  getPathInfo;    /**< Virtual get file information routine */
    MprGetPathLinkProc  getPathLink;    /**< Virtual get the symbolic link target */
    MprListDirProc      listDir;        /**< Virtual get directory list */
    MprMakeDirProc      makeDir;        /**< Virtual make directory routine */
    MprMakeLinkProc     makeLink;       /**< Virtual make link routine */
    MprOpenFileProc     openFile;       /**< Virtual open file routine */
    MprCloseFileProc    closeFile;      /**< Virtual close file routine */
    MprReadFileProc     readFile;       /**< Virtual read file routine */
    MprSeekFileProc     seekFile;       /**< Virtual seek file routine */
    MprSetBufferedProc  setBuffered;    /**< Virtual set buffered I/O routine */
    MprWriteFileProc    writeFile;      /**< Virtual write file routine */
    MprTruncateFileProc truncateFile;   /**< Virtual truncate file routine */
    bool                caseSensitive;  /**< Path comparisons are case sensitive */
    bool                hasDriveSpecs;  /**< Paths can have drive specifications */
    char                *separators;    /**< Filename path separators. First separator is the preferred separator. */
    char                *newline;       /**< Newline for text files */
    cchar               *root;          /**< Root file path */
#if ME_WIN_LIKE || CYGWIN
    char                *cygwin;        /**< Cygwin install directory */
    char                *cygdrive;      /**< Cygwin drive root */
#endif
} MprFileSystem;

/**
    Create and initialize the FileSystem subsystem.
    @description This is an internal routine called by the MPR during initialization.
    @param fs File system object.
    @param path Path name to the root of the file system.
    @ingroup MprFileSystem
    @stability Internal
 */
PUBLIC void mprInitFileSystem(MprFileSystem *fs, cchar *path);

/**
    A RomInode is created for each file in the Rom file system.
    @ingroup FileSystem
    @stability Internal
 */
typedef struct  MprRomInode {
    char            *path;              /**< File path */
    uchar           *data;              /**< Pointer to file data (unmanaged) */
    int             size;               /**< Size of file */
    int             num;                /**< Inode number */
} MprRomInode;

typedef struct MprRomFileSystem {
    MprFileSystem   fileSystem;         /**< Extends MprFileSystem */
    MprHash         *fileIndex;
    MprRomInode     *romInodes;         /**< File inode data (unmanaged) */
} MprRomFileSystem;

#if ME_ROM
/**
    Create and initialize the ROM FileSystem.
    @description This is an internal routine called by the MPR during initialization.
    @param path Path name to the root of the file system.
    @param inodes File definitions
    @return Returns a new file system object
    @ingroup MprFileSystem
    @stability Internal
*/
PUBLIC MprRomFileSystem *mprCreateRomFileSystem(cchar *path, MprRomInode *inodes);

/**
    Get the ROM file system data
    @return Returns a pointer to the list of ROM inodes.
    @ingroup MprFileSystem
    @stability Evolving
 */
PUBLIC MprRomInode *mprGetRomFiles(void);
#endif /* ME_ROM */

typedef MprFileSystem MprDiskFileSystem;
/**
    Create and initialize the disk FileSystem.
    @description This is an internal routine called by the MPR during initialization.
    @param path Path name to the root of the file system.
    @return Returns a new file system object
    @ingroup MprFileSystem
    @stability Internal
 */
PUBLIC MprDiskFileSystem *mprCreateDiskFileSystem(cchar *path);

/**
    Create and initialize the disk FileSystem.
    @description This is an internal routine called by the MPR during initialization.
    @param fs File system object
    @ingroup MprFileSystem
    @stability Internal
 */
PUBLIC void mprAddFileSystem(MprFileSystem *fs);

/**
    Lookup a file system
    @param path Path representing a file in the file system.
    @return Returns a file system object.
    @ingroup MprFileSystem
    @stability Internal
  */
PUBLIC MprFileSystem *mprLookupFileSystem(cchar *path);

/**
    Set the file system path separators
    @param path Path representing a file in the file system.
    @param separators String containing the directory path separators. Defaults to "/". Windows uses "/\/".
    @ingroup MprFileSystem
    @stability Stable
 */
PUBLIC void mprSetPathSeparators(cchar *path, cchar *separators);

/**
    Set the file system new line character string
    @param path Path representing a file in the file system.
    @param newline String containing the newline character(s). "\\n". Windows uses "\\r\\n".
    @ingroup MprFileSystem
    @stability Stable
 */
PUBLIC void mprSetPathNewline(cchar *path, cchar *newline);

PUBLIC MprList *mprGetDirList(cchar *path);

/**
    File I/O Module
    @description MprFile is the cross platform File I/O abstraction control structure. An instance will be
         created when a file is created or opened via #mprOpenFile.
         Note: Individual files are not thread-safe and should only be used by one file.
    @stability Stable.
    @see MprFile mprAttachFileFd mprCloseFile mprDisableFileBuffering mprEnableFileBuffering mprFlushFile mprGetFileChar
        mprGetFilePosition mprGetFileSize mprGetStderr mprGetStdin mprGetStdout mprOpenFile
        mprPeekFileChar mprPutFileChar mprPutFileString mprReadFile mprReadLine mprSeekFile mprTruncateFile mprWriteFile
        mprWriteFileFmt mprWriteFileString
        mprGetFileFd
    @defgroup MprFile MprFile
 */
typedef struct MprFile {
    char            *path;              /**< Filename */
    MprFileSystem   *fileSystem;        /**< File system owning this file */
    MprBuf          *buf;               /**< Buffer for I/O if buffered */
    MprOff          pos;                /**< Current read position  */
    MprOff          iopos;              /**< Raw I/O position  */
    MprOff          size;               /**< Current file size */
    int             mode;               /**< File open mode */
    int             perms;              /**< File permissions */
    int             fd;                 /**< File handle */
    int             attached;           /**< Attached to existing descriptor */
    MprRomInode     *inode;             /**< Reference to ROM file */
} MprFile;


/**
    Attach to an existing file descriptor
    @description Attach a file to an open file decriptor and return a file object.
    @param fd File descriptor to attach to
    @param name Descriptive name for the file.
    @param omode Posix style file open mode mask. The open mode may contain
        the following mask values ored together:
        @li O_RDONLY Open read only
        @li O_WRONLY Open write only
        @li O_RDWR Open for read and write
        @li O_CREAT Create or re-create
        @li O_TRUNC Truncate
        @li O_BINARY Open for binary data
        @li O_TEXT Open for text data
        @li O_EXCL Open with an exclusive lock
        @li O_APPEND Open to append
    @return Returns an MprFile object to use in other file operations.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC MprFile *mprAttachFileFd(int fd, cchar *name, int omode);

/**
    Close a file
    @description This call closes a file without destroying the file object.
    @param file File instance returned from #mprOpenFile
    @return Returns zero if successful, otherwise a negative MPR error code..
    @ingroup MprFile
    @stability Stable
*/
PUBLIC int mprCloseFile(MprFile *file);

/**
    Disable file buffering
    @description Disable any buffering of data when using the buffer.
    @param file File instance returned from #mprOpenFile
    @ingroup MprFile
    @stability Stable
 */
PUBLIC void mprDisableFileBuffering(MprFile *file);

/**
    Enable file buffering
    @description Enable data buffering when using the buffer.
    @param file File instance returned from #mprOpenFile
    @param size Size to allocate for the buffer.
    @param maxSize Maximum size the data buffer can grow to
    @ingroup MprFile
    @stability Stable
 */
PUBLIC int mprEnableFileBuffering(MprFile *file, ssize size, ssize maxSize);

/**
    Flush any buffered write data
    @description Write buffered write data and then reset the internal buffers.
    @param file Pointer to an MprFile object returned via MprOpen.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC int mprFlushFile(MprFile *file);

/**
    Read a character from the file.
    @description Read a single character from the file and advance the read position.
    @param file Pointer to an MprFile object returned via MprOpen.
    @return If successful, return the character just read. Otherwise return a negative MPR error code.
        End of file is signified by reading 0.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC int mprGetFileChar(MprFile *file);

/**
    Get the file descriptor for a file
    @param file File object returned via #mprOpenFile
    @return An integer O/S file descriptor
    @ingroup MprFile
    @stability Stable
 */
PUBLIC int mprGetFileFd(MprFile *file);

/**
    Return the current file position
    @description Return the current read/write file position.
    @param file A file object returned from #mprOpenFile
    @returns The current file offset position if successful. Returns a negative MPR error code on errors.
    @ingroup MprFile
 */
PUBLIC MprOff mprGetFilePosition(MprFile *file);

/**
    Get the size of the file
    @description Return the current file size
    @param file A file object returned from #mprOpenFile
    @returns The current file size if successful. Returns a negative MPR error code on errors.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC MprOff mprGetFileSize(MprFile *file);

/**
    Return a file object for the Stderr I/O channel
    @returns A file object
    @ingroup MprFile
    @stability Stable
 */
PUBLIC MprFile *mprGetStderr(void);

/**
    Return a file object for the Stdin I/O channel
    @returns A file object
    @ingroup MprFile
    @stability Stable
 */
PUBLIC MprFile *mprGetStdin(void);

/**
    Return a file object for the Stdout I/O channel
    @returns A file object
    @ingroup MprFile
    @stability Stable
 */
PUBLIC MprFile *mprGetStdout(void);

/**
    Open a file
    @description Open a file and return a file object.
    @param filename String containing the filename to open or create.
    @param omode Posix style file open mode mask. The open mode may contain
        the following mask values ored together:
        @li O_RDONLY Open read only
        @li O_WRONLY Open write only
        @li O_RDWR Open for read and write
        @li O_CREAT Create file if it does not exist
        @li O_TRUNC Truncate size to zero length
        @li O_BINARY Open for binary data
        @li O_TEXT Open for text data
        @li O_EXCL Open with an exclusive lock
        @li O_APPEND Open to append
    @param perms Posix style file permissions mask.
    @return Returns an MprFile object to use in other file operations.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC MprFile *mprOpenFile(cchar *filename, int omode, int perms);

/**
    Non-destructively read a character from the file.
    @description Read a single character from the file without advancing the read position.
    @param file Pointer to an MprFile object returned via MprOpen.
    @return If successful, return the character just read. Otherwise return a negative MPR error code.
        End of file is signified by reading 0.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC int mprPeekFileChar(MprFile *file);

/**
    Write a character to the file.
    @description Writes a single character to the file. Output is buffered and is
        flushed as required or when mprClose is called.
    @param file Pointer to an MprFile object returned via MprOpen.
    @param c Character to write
    @return One if successful, otherwise returns a negative MPR error code on errors.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC ssize mprPutFileChar(MprFile *file, int c);

/**
    Write a string to the file.
    @description Writes a string to the file. Output is buffered and is flushed as required or when mprClose is called.
    @param file Pointer to an MprFile object returned via MprOpen.
    @param str String to write
    @return The number of characters written to the file. Returns a negative MPR error code on errors.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC ssize mprPutFileString(MprFile *file, cchar *str);

/**
    Read data from a file.
    @description Reads data from a file.
    @param file Pointer to an MprFile object returned via MprOpen.
    @param buf Buffer to contain the read data.
    @param size Size of \a buf in characters.
    @return The number of characters read from the file. Returns a negative MPR error code on errors.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC ssize mprReadFile(MprFile *file, void *buf, ssize size);

/**
    Read a line from the file.
    @description Read a single line from the file. Lines are delimited by the newline character. The newline is not
        included in the returned buffer. This call will read lines up to the given size in length. If no newline is
        found, all available characters, up to size, will be returned.
    @param file Pointer to an MprFile object returned via MprOpen.
    @param size Maximum number of characters in a line.
    @param len Pointer to an integer to hold the length of the returned string.
    @return An allocated string and sets *len to the number of bytes read.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC char *mprReadLine(MprFile *file, ssize size, ssize *len);

/**
    Seek the I/O pointer to a new location in the file.
    @description Move the position in the file to/from which I/O will be performed in the file. Seeking prior
        to a read or write will cause the next I/O to occur at that location.
    @param file Pointer to an MprFile object returned via MprOpen.
    @param seekType Seek type may be one of the following three values:
        @li SEEK_SET    Seek to a position relative to the start of the file
        @li SEEK_CUR    Seek relative to the current position
        @li SEEK_END    Seek relative to the end of the file
    @param distance A positive or negative byte offset.
    @return Returns the new file position if successful otherwise a negative MPR error code is returned.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC MprOff mprSeekFile(MprFile *file, int seekType, MprOff distance);

/**
    Truncate a file
    @description Truncate a file to a given size. Note this works on a path and not on an open file.
    @param path File to truncate
    @param size New maximum size for the file.
    @returns Zero if successful.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC int mprTruncateFile(cchar *path, MprOff size);

/**
    Write data to a file.
    @description Writes data to a file.
    @param file Pointer to an MprFile object returned via MprOpen.
    @param buf Buffer containing the data to write.
    @param count Cound of characters in \a buf to write
    @return The number of characters actually written to the file. Returns a negative MPR error code on errors.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC ssize mprWriteFile(MprFile *file, cvoid *buf, ssize count);

/**
    Write formatted data to a file.
    @description Writes a formatted string to a file.
    @param file Pointer to an MprFile object returned via MprOpen.
    @param fmt Format string
    @return The number of characters actually written to the file. Returns a negative MPR error code on errors.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC ssize mprWriteFileFmt(MprFile *file, cchar *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/**
    Write a string to a file.
    @description Writes a string to a file.
    @param file Pointer to an MprFile object returned via MprOpen.
    @param str String to write
    @return The number of characters actually written to the file. Returns a negative MPR error code on errors.
    @ingroup MprFile
    @stability Stable
 */
PUBLIC ssize mprWriteFileString(MprFile *file, cchar *str);

/*********************************** Paths ************************************/
/**
    Path (filename) Information
    @description MprPath is the cross platform Path (filename) information structure.
    @stability Internal.
    @see MprDirEntry MprFile MprPath mprCopyPath mprDeletePath mprGetAbsPath mprGetCurrentPath
        mprGetFirstPathSeparator mprGetLastPathSeparator mprGetNativePath mprGetPathBase
        mprGetPathDir mprGetPathExt mprGetPathFiles mprGetPathLink mprGetPathNewline mprGetPathParent
        mprGetPathSeparators mprGetPortablePath mprGetRelPath mprGetTempPath mprGetWinPath mprIsPathAbs
        mprIsRelPath mprJoinPath mprJoinPaths mprJoinPathExt mprMakeDir mprMakeLink mprMapSeparators mprNormalizePath
        mprPathExists mprReadPathContents mprReplacePathExt mprResolvePath mprSamePath mprSamePathCount mprSearchPath
        mprTransformPath mprTrimPathExt mprTruncatePath
    @defgroup MprPath MprPath
 */
typedef struct MprPath {
    MprTime         atime;              /**< Access time */
    MprTime         ctime;              /**< Create time */
    MprTime         mtime;              /**< Modified time */
    MprOff          size;               /**< File length */
    int64           inode;              /**< Inode number */
    int             perms;              /**< Permission mask */
    int             owner;              /**< Owner ID */
    int             group;              /**< Group ID */

    bool            checked: 1;         /**< Path has been checked */
    bool            isDir: 1;           /**< Set if directory */
    bool            isLink: 1;          /**< Set if a symbolic link  */
    bool            isReg: 1;           /**< Set if a regular file */
    bool            caseMatters: 1;     /**< Case comparisons matter */
    bool            valid: 1;           /**< Valid data bit */
} MprPath;

/**
    Directory entry description
    @description The MprGetDirList will create a list of directory entries.
    @ingroup MprPath
    @stability Internal
 */
typedef struct MprDirEntry {
    char            *name;              /**< Name of the file */
    MprTime         lastModified;       /**< Time the file was last modified */
    MprOff          size;               /**< Size of the file */
    bool            isDir: 1;           /**< True if the file is a directory */
    bool            isLink: 1;          /**< True if the file is a symbolic link */
} MprDirEntry;

/*
    Search path separator
 */
#if ME_WIN_LIKE
    #define MPR_SEARCH_SEP      ";"
    #define MPR_SEARCH_SEP_CHAR ';'
#else
    #define MPR_SEARCH_SEP      ":"
    #define MPR_SEARCH_SEP_CHAR ':'
#endif

/**
    Copy a file
    @description Create a new copy of a file with the specified open permissions mode.
    @param from Path of the existing file to copy
    @param to Name of the new file copy
    @param omode Posix style file open mode mask. See #mprOpenFile for the various modes.
    @returns True if the file exists and can be accessed
    @ingroup MprPath
    @stability Stable
 */
PUBLIC int mprCopyPath(cchar *from, cchar *to, int omode);

/**
    Delete a file.
    @description Delete a file or directory.
    @param path String containing the path to delete.
    @return Returns zero if successful otherwise a negative MPR error code is returned.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC int mprDeletePath(cchar *path);

/**
    Convert a path to an absolute path
    @description Get an absolute (canonical) equivalent representation of a path. On windows this path will have
        back-slash directory separators and will have a drive specifier. On Cygwin, the path will be a Cygwin style
        path with forward-slash directory specifiers and without a drive specifier. If the path is outside the
        cygwin filesystem (outside c:/cygwin), the path will have a /cygdrive/DRIVE prefix. To get a windows style
        path on *NIX, use mprGetWinPath.
    @param path Path to examine
    @returns An absolute path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetAbsPath(cchar *path);

/**
    Return the current working directory
    @return Returns an allocated string with the current working directory as an absolute path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC cchar *mprGetCurrentPath(void);

/**
    Get the first path separator in a path
    @param path Path to examine
    @return Returns a reference to the first path separator in the given path
    @ingroup MprPath
    @stability Stable
 */
PUBLIC cchar *mprGetFirstPathSeparator(cchar *path);

/*
    Get the last path separator in a path
    @param path Path to examine
    @return Returns a reference to the last path separator in the given path
    @ingroup MprPath
    @stability Stable
 */
PUBLIC cchar *mprGetLastPathSeparator(cchar *path);

/**
    Get a path formatted according to the native O/S conventions.
    @description Get an equivalent absolute path formatted using the directory separators native to the O/S platform.
    On Windows, it will use backward slashes ("\") as the directory separator and will contain a drive specification.
    @param path Path name to examine
    @returns An allocated string containing the new path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetNativePath(cchar *path);

/**
    Get the base portion of a path
    @description Get the base portion of a path by stripping off all directory components
    @param path Path name to examine
    @returns A path without any directory portion.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetPathBase(cchar *path);

/**
    Get a reference to the base portion of a path
    @description Get the base portion of a path by stripping off all directory components. This returns a reference
        into the original path.
    @param path Path name to examine
    @returns A path without any directory portion. The path is a reference into the original file string.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC cchar *mprGetPathBaseRef(cchar *path);

/**
    Get the directory portion of a path
    @description Get the directory portion of a path by stripping off the base name.
    @param path Path name to examine
    @returns A new string containing the directory name.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetPathDir(cchar *path);

/**
    Get the file extension portion of a path
    @description Get the file extension portion of a path. The file extension is the portion starting with the last "."
        in the path. It thus does not include the "." as the first charcter.
    @param path Path name to examine
    @returns A path extension without the ".". Returns null if no extension exists.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetPathExt(cchar *path);

/*
    Flags for mprGetPathFiles
 */
#define MPR_PATH_DESCEND        0x1             /**< Flag for mprGetPathFiles to traverse subdirectories */
#define MPR_PATH_DEPTH_FIRST    0x2             /**< Flag for mprGetPathFiles to do a depth-first traversal */
#define MPR_PATH_INC_HIDDEN     0x4             /**< Flag for mprGetPathFiles to include hidden files */
#define MPR_PATH_NO_DIRS        0x8             /**< Flag for mprGetPathFiles to exclude subdirectories */
#define MPR_PATH_RELATIVE       0x10            /**< Flag for mprGetPathFiles to return paths relative to the directory */

/**
    Create a list of files in a directory or subdirectories. This call returns a list of MprDirEntry objects.
    @description Get the list of files in a directory and return a list.
    @param dir Directory to list.
    @param flags The flags may be set to #MPR_PATH_DESCEND to traverse subdirectories. This effectively appends
        '**' to the path. Set #MPR_PATH_NO_DIRS to exclude directories from the results. Set to MPR_PATH_HIDDEN
        to include hidden files that start with ".". Set to MPR_PATH_DEPTH_FIRST to do a depth-first traversal,
        i.e. traverse subdirectories before considering adding the directory to the list.
    @returns A list (MprList) of MprDirEntry objects.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC MprList *mprGetPathFiles(cchar *dir, int flags);

/**
    Create a list of files in a directory or subdirectories that match the given wildcard pattern.
        This call returns a list of filenames.
    @description Get the list of files in a directory and return a list. The pattern list may contain
    wild cards: "?" Matches any single character, "*" matches zero or more characters of the file or directory,
    "**"/ matches zero or more directories, "**" matches zero or more files or directories.
    An exclusion pattern may be specified to apply to subsequent patterns by appending with "!".
    @param path Directory to list.
    @param patterns Wild card pattern to match.
    @param flags Set to MPR_PATH_HIDDEN to include hidden files that start with ".". Set to MPR_PATH_DEPTH_FIRST to do a
        depth-first traversal, i.e. traverse subdirectories before considering adding the directory to the list.
        Set MPR_PATH_RELATIVE to return files relative to the given path. Set MPR_PATH_NO_DIRS to omit directories.
    @returns A list (MprList) of filenames.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC MprList *mprGlobPathFiles(cchar *path, cchar *patterns, int flags);

/**
    Get the first directory portion of a path
    @param path Path name to examine
    @returns A new string containing the directory name.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetPathFirstDir(cchar *path);

/**
    Return information about a file represented by a path.
    @description Returns file status information regarding the \a path.
    @param path String containing the path to query.
    @param info Pointer to a pre-allocated MprPath structure.
    @return Returns zero if successful, otherwise a negative MPR error code is returned.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC int mprGetPathInfo(cchar *path, MprPath *info);

/**
    Get the target of a symbolic link.
    @description Return the path pointed to by a symbolic link. Not all platforms support symbolic links.
    @param path Path name to examine
    @returns A path representing the target of the symbolic link.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetPathLink(cchar *path);

/**
    Get the file newline character string for a given path.
    Return the character string used to delimit new lines in text files.
    @param path Use this path to specify either the root of the file system or a file on the file system.
    @returns A string used to delimit new lines. This is typically "\n" or "\r\n"
    @ingroup MprPath
    @stability Stable
 */
PUBLIC cchar *mprGetPathNewline(cchar *path);

/**
    Get the parent directory of a path
    @param path Path name to examine
    @returns An allocated string containing the parent directory.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetPathParent(cchar *path);

/**
    Get the path directory separator.
    Return the directory separator characters used to separate directories on a given file system. Typically "/" or "\"
        The first entry is the default separator.
    @param path Use this path to specify either the root of the file system or a file on the file system.
    @returns The string of path separators. The first entry is the default separator.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC cchar *mprGetPathSeparators(cchar *path);

/**
    Get the default path directory separator.
    Return the default directory separator character used to separate directories on a given file system.
        Typically "/" or "\".
    @param path Use this path to specify either the root of the file system or a file on the file system.
    @returns Character path separator
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char mprGetPathSeparator(cchar *path);

/**
    Get a portable path
    @description Get an equivalent absolute path that is somewhat portable.
        This means it will use forward slashes ("/") as the directory separator. This call will not remove drive specifiers.
    @param path Path name to examine
    @returns An allocated string containing the new path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetPortablePath(cchar *path);

/**
    Get a path relative to another path.
    @description Get a relative path path from an origin path to a destination. If a relative path cannot be obtained,
        an absolute path to the destination will be returned. This happens if the paths cross drives.
    @param dest Destination file
    @param origin Starting location from which to compute a relative path to the destination
        If the origin is null, use the application's current working directory as the origin.
    @returns An allocated string containing the relative directory.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetRelPath(cchar *dest, cchar *origin);

/**
    Make a temporary file.
    @description Thread-safe way to make a unique temporary file.
    @param tmpDir Base directory in which the temp file will be allocated.
    @return An allocated string containing the path of the temp file.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetTempPath(cchar *tmpDir);

/**
    Convert a path to an absolute windows path
    @description Get a windows style, absolute (canonical) equivalent representation of a path. This path will
        have back-slash delimiters and a drive specifier. On non-windows systems, this returns an absolute path using
        mprGetAbsPath.
    @param path Path to examine
    @returns A windows-style absolute path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprGetWinPath(cchar *path);

/**
    Determine if a directory is the same as or a parent of a path.
    @param dir Directory to examine if it is a parent of path.
    @param path Path name to examine
    @returns True if directory is a parent of the path or is the same as the given path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC bool mprIsPathContained(cchar *path, cchar *dir);

/**
    Fast version of mprIsPathContained that works only for absolute paths.
    Determine if a directory is the same as or a parent of a path.
    @param path Path name to examine
    @param dir Directory to examine if it is a parent of path or equal to path
    @returns True if directory is a parent of the path or is the same as the given path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC bool mprIsAbsPathContained(cchar *path, cchar *dir);

/**
    Determine if a path is absolute
    @param path Path name to examine
    @returns True if the path is absolue
    @ingroup MprPath
    @stability Stable
 */
PUBLIC bool mprIsPathAbs(cchar *path);

/**
    Determine if a path is a directory
    @param path Path name to examine
    @returns True if the path is a directory
    @ingroup MprPath
    @stability Stable
 */
PUBLIC bool mprIsPathDir(cchar *path);

/**
    Determine if a path is relative
    @param path Path name to examine
    @returns True if the path is relative
    @ingroup MprPath
    @stability Stable
 */
PUBLIC bool mprIsPathRel(cchar *path);

/**
    Test if a character is a path separarator
    @param path Path name to identify the file system
    @param c Character to test
    @return Returns true if the character is a path separator on the file system containing the given path
    @ingroup MprPath
    @stability Stable
 */
PUBLIC bool mprIsPathSeparator(cchar *path, cchar c);

/**
    Join paths
    @description Join a path to a base path. If path is absolute, it will be returned.
    @param base Directory path name to use as the base.
    @param path Other path name to join to the base path.
    @returns Allocated string containing the resolved path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprJoinPath(cchar *base, cchar *path);

/**
    Join paths
    @description Join each given path in turn to the path. Calls mprJoinPath for each argument.
    @param base Directory path name to use as the base.
    @param ... Other paths to join to the base path. List of other paths must be NULL terminated.
    @returns Allocated string containing the resolved path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprJoinPaths(cchar *base, ...);

/**
    Join an extension to a path
    @description Add an extension to a path if it does not already have one.
    @param path Path name to use as a base. Path is not modified.
    @param ext Extension to add. Must should not have a  period prefix.
    @returns Allocated string containing the resolved path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprJoinPathExt(cchar *path, cchar *ext);

/**
    Make a directory
    @description Make a directory using the supplied path. Intermediate directories are created as required.
    @param path String containing the directory pathname to create.
    @param makeMissing If true make all required intervening directory segments.
    @param perms Posix style file permissions mask.
    @param owner User to own the directory. Set to -1 not change the owner.
    @param group Group to own the directory. Set to -1 not change the group.
    @return Returns zero if successful, otherwise a negative MPR error code is returned.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC int mprMakeDir(cchar *path, int perms, int owner, int group, bool makeMissing);

/**
    Make a link
    @description Make a link at the target to the specified path. This will make symbolic or hard links
        depending on the value of the hard parameter
    @param path String containing the path to link to
    @param target String containing the new link path to be created.
    @param hard If true, make a hard link, otherwise make a soft link.
    @return Returns zero if successful, otherwise a negative MPR error code is returned.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC int mprMakeLink(cchar *path, cchar *target, bool hard);

/**
    Map the separators in a path.
    @description Map the directory separators in a path to the specified separators. This is useful to change from
        backward to forward slashes when dealing with Windows paths.
    @param path Path name to examine
    @param separator Separator character to use.
    @returns An allocated string containing the parent directory.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC void mprMapSeparators(char *path, int separator);

/**
    Normalize a path
    @description A path is normalized by redundant segments such as "./" and "../dir" and duplicate
        path separators. Path separators are mapped. Paths are not converted to absolute paths.
    @param path First path to compare
    @returns A newly allocated, clean path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprNormalizePath(cchar *path);

/**
    Determine if a file exists for a path name and can be accessed
    @description Test if a file can be accessed for a given mode
    @param path Path name to test
    @param omode Posix style file open mode mask. See #mprOpenFile for the various modes.
    @returns True if the file exists and can be accessed
    @ingroup MprPath
    @stability Stable
 */
PUBLIC bool mprPathExists(cchar *path, int omode);

/*
    Read the contents of a file
    @param path Filename to open and read
    @param lenp Optional pointer to a ssize integer to contain the length of the returns data string. Set to NULL if not
        required.
    @return An allocated string containing the file contents and return the data length in lenp.
        Returns null if the fail cannot be opened or read.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprReadPathContents(cchar *path, ssize *lenp);

/**
    Replace an extension to a path
    @description Remove any existing path extension and then add the given path extension.
    @param path Path filename to modify
    @param ext Extension to add. The extension should not have a period prefix.
    @returns Allocated string containing the resolved path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprReplacePathExt(cchar *path, cchar *ext);

/**
    Resolve paths
    @description Resolve paths in the neighborhood of this path. Resolve operates like join, except that it joins the
    given paths to the directory portion of the current ("this") path. For example:
    Path("/usr/bin/ejs/bin").resolve("lib") will return "/usr/lib/ejs/lib". i.e. it will return the
    sibling directory "lib".
    \n\n
    Resolve operates by determining a virtual current directory for this Path object. It then successively
    joins the given paths to the directory portion of the current result. If the next path is an absolute path,
    it is used unmodified.  The effect is to find the given paths with a virtual current directory set to the
    directory containing the prior path.
    \n\n
    Resolve is useful for creating paths in the region of the current path and gracefully handles both
    absolute and relative path segments.
    \n\n
    Returns a joined (normalized) path.
    If path is absolute, then return path. If path is null, empty or "." then return path.
    @param base Base path to use as the base.
    @param path Path name to resolve against base.
    @returns Allocated string containing the resolved path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprResolvePath(cchar *base, cchar *path);

/**
    Compare two paths if they are the same
    @description Compare two paths to see if they are equal. This normalizes the paths to absolute paths first before
        comparing. It does handle case sensitivity appropriately.
    @param path1 First path to compare
    @param path2 Second path to compare
    @returns True if the file exists and can be accessed
    @ingroup MprPath
    @stability Stable
 */
PUBLIC int mprSamePath(cchar *path1, cchar *path2);

/**
    Compare two paths if they are the same for a given length.
    @description Compare two paths to see if they are equal. This normalizes the paths to absolute paths first before
        comparing. It does handle case sensitivity appropriately. The len parameter
        if non-zero, specifies how many characters of the paths to compare.
    @param path1 First path to compare
    @param path2 Second path to compare
    @param len How many characters to compare.
    @returns True if the file exists and can be accessed
    @ingroup MprPath
    @stability Stable
 */
PUBLIC int mprSamePathCount(cchar *path1, cchar *path2, ssize len);

/*
    Flags for mprSearchPath
 */
#define MPR_SEARCH_EXE      0x1         /* Search for an executable */
#define MPR_SEARCH_DIR      0x2         /* Search for a directory */
#define MPR_SEARCH_FILE     0x4         /* Search for regular file */

/**
    Search for a path
    @description Search for a file using a given set of search directories
    @param path Path name to locate. Must be an existing file or directory.
    @param flags Flags.
    @param search Variable number of directories to search.
    @returns Allocated string containing the full path name of the located file.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprSearchPath(cchar *path, int flags, cchar *search, ...);

/*
    Flags for mprTransformPath
 */
#define MPR_PATH_ABS            0x1     /* Normalize to an absolute path */
#define MPR_PATH_REL            0x2     /* Normalize to an relative path */
#define MPR_PATH_WIN            0x4     /* Normalize to a windows path */
#define MPR_PATH_NATIVE_SEP     0x8     /* Use native path separators */

/**
    Transform a path
    @description A path is transformed by cleaning and then transforming according to the flags.
    @param path First path to compare
    @param flags Flags to modify the path representation.
    @returns A newly allocated, clean path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprTransformPath(cchar *path, int flags);

/**
    Trim path components from a path
    @description Trim the requested number of path components from the front or end of a path
    @param path Path to examine
    @param count Number of components to trim. If negative, trim from the end.
    @returns An allocated string with the trimmed path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprTrimPathComponents(cchar *path, int count);

/**
    Trim an extension from a path
    @description Trim a file extension (".ext") from a path name.
    @param path Path to examine
    @returns An allocated string with the trimmed path.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprTrimPathExt(cchar *path);

/**
    Trim the drive from a path
    @description Trim a drive specifier ("c:") from the start of a path.
    @param path Path to examine
    @returns An allocated string with the trimmed drive.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC char *mprTrimPathDrive(cchar *path);

/**
    Create a file and write contents
    @description The file is created, written and closed. If the file already exists, it is recreated.
    @param path Filename to create
    @param buf Buffer of data to write to the file
    @param len Size of the buf parameter in bytes
    @param mode File permissions with which to create the file. E.g. 0644.
    @return The number of bytes written. Should equal len. Otherwise return a negative MPR error code.
    @ingroup MprPath
    @stability Stable
 */
PUBLIC ssize mprWritePathContents(cchar *path, cchar *buf, ssize len, int mode);

/*
    Internal - prototype
 */
PUBLIC bool mprMatchPath(cchar *path, cchar *pattern);

/********************************** O/S Dep ***********************************/
/**
    Create and initialze the O/S dependent subsystem
    @description Called internally by the MPR. Should not be called by users.
    @ingroup Mpr
    @stability Internal
 */
PUBLIC int mprCreateOsService(void);

/**
    Start the O/S dependent subsystem
    @ingroup Mpr
    @stability Internal
 */
PUBLIC int mprStartOsService(void);

/**
    Stop the O/S dependent subsystem
    @ingroup Mpr
    @stability Internal
 */
PUBLIC void mprStopOsService(void);

/********************************* Modules ************************************/
/**
    Loadable module service
    @see mprCreateModuleService mprStartModuleService mprStopModuleService
    @defgroup MprModuleSerivce MprModuleService
    @stability Internal
 */
typedef struct MprModuleService {
    MprList     *modules;               /**< List of defined modules */
    char        *searchPath;            /**< Module search path to locate modules */
    MprMutex    *mutex;
} MprModuleService;


/**
    Create and initialize the module service
    @return MprModuleService object
    @ingroup MprModuleService
    @stability Internal
 */
PUBLIC MprModuleService *mprCreateModuleService(void);

/**
    Start the module service
    @description This calls the start entry point for all registered modules
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup MprModuleService
    @stability Internal
 */
PUBLIC int mprStartModuleService(void);

/**
    Stop the module service
    @description This calls the stop entry point for all registered modules
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup MprModuleService
    @stability Internal
 */
PUBLIC void mprStopModuleService(void);

/**
    Module start/stop point function signature
    @param mp Module object reference returned from #mprCreateModule
    @returns zero if successful, otherwise return a negative MPR error code.
    @ingroup MprModule
    @stability Stable
 */
typedef int (*MprModuleProc)(struct MprModule *mp);

/*
    Module flags
 */
#define MPR_MODULE_STARTED     0x1     /**< Module stared **/
#define MPR_MODULE_STOPPED     0x2     /**< Module stopped */

/**
    Loadable Module Service
    @description The MPR provides services to load and unload shared libraries.
    @see MprModule MprModuleEntry MprModuleProc mprCreateModule mprGetModuleSearchPath mprLoadModule mprLoadNativeModule
        mprLookupModule mprLookupModuleData mprSearchForModule mprSetModuleFinalizer mprSetModuleSearchPath
        mprSetModuleTimeout mprStartModule mprStopModule mprUnloadModule mprUnloadNativeModule
    @stability Stable.
    @defgroup MprModule MprModule
    @stability Internal
 */
typedef struct MprModule {
    char            *name;              /**< Unique module name */
    char            *path;              /**< Module library filename */
    char            *entry;             /**< Module library init entry point */
    void            *moduleData;        /**< Module specific data - not managed */
    void            *handle;            /**< O/S shared library load handle */
    MprTime         modified;           /**< When the module file was last modified */
    MprTicks        lastActivity;       /**< When the module was last used */
    MprTicks        timeout;            /**< Inactivity unload timeout */
    int             flags;              /**< Module control flags */
    MprModuleProc   start;              /**< Start the module */
    MprModuleProc   stop;               /**< Stop the module. Should be unloadable after stopping */
} MprModule;

/**
    Loadable module entry point signature.
    @description Loadable modules can have an entry point that is invoked automatically when a module is loaded.
    @param data Data passed to mprCreateModule
    @param mp Module object reference returned from #mprCreateModule
    @return a new MprModule structure for the module. Return NULL if the module cannot be initialized.
    @ingroup MprModule
    @stability Stable
 */
typedef int (*MprModuleEntry)(void *data, MprModule *mp);

/**
    Create a module
    @description This call will create a module object for a loadable module. This should be invoked by the
        module itself in its module entry point to register itself with the MPR.
    @param name Name of the module
    @param path Optional filename of a module library to load. When loading, the filename will be searched using
        the defined module search path (see #mprSetModuleSearchPath). The filename may or may not include a platform
        specific shared library extension such as .dll, .so or .dylib. By omitting the library extension, code can
        portably load shared libraries.
    @param entry Name of function to invoke after loading the module.
    @param data Arbitrary data pointer. This will be defined in MprModule.data and passed into the module initialization
        entry point.
    @returns A module object for this module
    @ingroup MprModule
    @stability Stable
 */
PUBLIC MprModule *mprCreateModule(cchar *name, cchar *path, cchar *entry, void *data);

/**
    Get the module search path
    @description Get the directory search path used by the MPR when loading dynamic modules. This is a colon separated (or
        semicolon on Windows) set of directories.
    @returns The module search path.
    @ingroup MprModule
    @stability Stable
 */
PUBLIC cchar *mprGetModuleSearchPath(void);

/**
    Load a module
    @description Load a module library. This will load a dynamic shared object (shared library) and call the
        modules library entry point. If the module is already loaded, this call will do nothing.
    @param mp Module object created via #mprCreateModule.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup MprModule
    @stability Stable
 */
PUBLIC int mprLoadModule(MprModule *mp);

#if ME_COMPILER_HAS_DYN_LOAD || DOXYGEN
/**
    Load a native module
    @param mp Module object created via #mprCreateModule.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup MprModule
    @stability Stable
*/
PUBLIC int mprLoadNativeModule(MprModule *mp);

/**
    Unload a native module
    @description WARNING: modules must be designed to be unloaded and must be quiesced before unloading.
    @param mp Module object created via #mprCreateModule.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup MprModule
    @stability Stable
*/
PUBLIC int mprUnloadNativeModule(MprModule *mp);
#endif

/**
    Lookup a module
    @description Lookup a module by name and return the module object.
    @param name Name of the module specified to #mprCreateModule.
    @returns A module object for this module created in the module entry point by calling #mprCreateModule
    @ingroup MprModule
    @stability Stable
 */
PUBLIC MprModule *mprLookupModule(cchar *name);

/**
    Lookup a module and return the module data
    @description Lookup a module by name and return the module specific data defined via #mprCreateModule.
    @param name Name of the module specified to #mprCreateModule.
    @returns The module data.
    @ingroup MprModule
    @stability Stable
 */
PUBLIC void *mprLookupModuleData(cchar *name);

/**
    Search for a module on the current module path
    @param module Name of the module to locate.
    @return A string containing the full path to the module. Returns NULL if the module filename cannot be found.
    @ingroup MprModule
    @stability Stable
 */
PUBLIC char *mprSearchForModule(cchar *module);

/**
    Define a module finalizer that will be called before a module is stopped
    @param module Module object to modify
    @param stop Callback function to invoke before stopping the module
    @ingroup MprModule
    @stability Stable
 */
PUBLIC void mprSetModuleFinalizer(MprModule *module, MprModuleProc stop);

/**
    Set the module search path
    @description Set the directory search path used by the MPR when loading dynamic modules. This path string must
        should be a colon separated (or semicolon on Windows) set of directories.
    @param searchPath Colon separated set of directories
    @returns The module search path.
    @ingroup MprModule
    @stability Stable
 */
PUBLIC void mprSetModuleSearchPath(char *searchPath);

/**
    Set a module timeout
    @param module Module object to modify
    @param timeout Inactivity timeout in milliseconds before unloading the module
    @ingroup MprModule
    @stability Internal
    @internal
 */
PUBLIC void mprSetModuleTimeout(MprModule *module, MprTicks timeout);

/**
    Start a module
    @description Invoke the module start entry point. The start routine is only called once.
    @param mp Module object returned via #mprLookupModule
    @ingroup MprModule
    @stability Internal
 */
PUBLIC int mprStartModule(MprModule *mp);

/**
    Stop a module
    @description Invoke the module stop entry point. The stop routine is only called once.
    @param mp Module object returned via #mprLookupModule
    @ingroup MprModule
    @stability Internal
 */
PUBLIC int mprStopModule(MprModule *mp);

/**
    Unload a module
    @description Unload a module from the MPR. This will unload a dynamic shared object (shared library). This routine
        is not fully supported by the MPR and is often fraught with issues. A module must usually be completely inactive
        with no allocated memory when it is unloaded. USE WITH CARE.
    @param mp Module object returned via #mprLookupModule
    @return Zero if the module can be unloaded. Otherwise a negative MPR error code.
    @ingroup MprModule
    @stability Internal
 */
PUBLIC int mprUnloadModule(MprModule *mp);

/********************************* Events *************************************/
/*
    Flags for mprCreateEvent
 */
#define MPR_EVENT_CONTINUOUS        0x1     /**< Timer event runs is automatically rescheduled */
#define MPR_EVENT_QUICK             0x2     /**< Execute inline without executing via a thread */
#define MPR_EVENT_DONT_QUEUE        0x4     /**< Don't queue the event. User must call mprQueueEvent. (internal use only) */
#define MPR_EVENT_STATIC_DATA       0x8     /**< Event data is permanent and should not be marked by GC */
#define MPR_EVENT_RUNNING           0x10    /**< Event currently executing */
#define MPR_EVENT_FOREIGN           0x20    /**< Invoking from a foreign non-mpr thread */

#define MPR_EVENT_MAX_PERIOD (MAXINT64 / 2)

/**
    Event callback function
    @return Return non-zero if the dispatcher is deleted. Otherwise return 0
    @ingroup MprEvent
    @stability Stable
 */
typedef void (*MprEventProc)(void *data, struct MprEvent *event);

/**
    Event object
    @description The MPR provides a powerful priority based eventing mechanism. Events are described by MprEvent objects
        which are created and queued via #mprCreateEvent. Each event may have a priority and may be one-shot or
        be continuously rescheduled according to a specified period. The event subsystem provides the basis for
        callback timers.
    @see MprDispatcher MprEvent MprEventProc MprEventService mprCreateDispatcher mprCreateEvent mprCreateEventService
        mprCreateTimerEvent mprDestroyDispatcher mprEnableContinuousEvent mprEnableDispatcher mprGetDispatcher
        mprQueueEvent mprRemoveEvent mprRescheduleEvent mprRestartContinuousEvent mprServiceEvents
        mprSignalDispatcher mprStopContinuousEvent mprWaitForEvent
    @defgroup MprEvent MprEvent
    @stability Internal
 */
typedef struct MprEvent {
    cchar               *name;          /**< Static debug name of the event */
    MprEventProc        proc;           /**< Callback procedure */
    MprTicks            timestamp;      /**< When was the event created */
    MprTicks            due;            /**< When is the event due */
    void                *data;          /**< Event private data (managed|unmanged depending on flags) */
    void                *sock;          /**< Optional socket data */
    int                 flags;          /**< Event flags */
    int                 mask;           /**< I/O mask of events */
    MprTicks            period;         /**< Reschedule period */
    struct MprEvent     *next;          /**< Next event linkage */
    struct MprEvent     *prev;          /**< Previous event linkage */
    struct MprDispatcher *dispatcher;   /**< Event dispatcher service */
    struct MprWaitHandler *handler;     /**< Optional wait handler */
    MprCond             *cond;          /**< Wait for event to complete */
} MprEvent;

/*
    Dispatcher flags
 */
#define MPR_DISPATCHER_IMMEDIATE  0x1   /**< Dispatcher should run using the service events thread */
#define MPR_DISPATCHER_WAITING    0x2   /**< Dispatcher waiting for an event in mprWaitForEvent */
#define MPR_DISPATCHER_DESTROYED  0x4   /**< Dispatcher has been destroyed */
#define MPR_DISPATCHER_AUTO       0x8   /**< Dispatcher was auto created in response to accept event */
#define MPR_DISPATCHER_COMPLETE   0x10  /**< Test operation is complete */

/**
    Event Dispatcher
    @defgroup MprDispatcher MprDispatcher
    @stability Internal
 */
typedef struct MprDispatcher {
    cchar           *name;              /**< Static debug dispatcher name / purpose */
    MprEvent        *eventQ;            /**< Event queue */
    MprEvent        *currentQ;          /**< Currently executing events */
    MprCond         *cond;              /**< Multi-thread sync */
    int             flags;              /**< Dispatcher control flags */
    int64           mark;               /**< Last event sequence mark (may reuse over time) */
    struct MprDispatcher *next;         /**< Next dispatcher linkage */
    struct MprDispatcher *prev;         /**< Previous dispatcher linkage */
    struct MprDispatcher *parent;       /**< Queue pointer */
    struct MprEventService *service;    /**< Event service reference */
    MprOsThread     owner;              /**< Thread currently dispatching events, otherwise zero */
} MprDispatcher;


/**
    Event Service
    @defgroup MprEvent MprEvent
    @stability Internal
 */
typedef struct MprEventService {
    MprTicks        now;                /**< Current notion of system time for the dispatcher service */
    MprTicks        willAwake;          /**< When the event service will next awake */
    MprDispatcher   *runQ;              /**< Queue of running dispatchers */
    MprDispatcher   *readyQ;            /**< Queue of dispatchers with events ready to run */
    MprDispatcher   *waitQ;             /**< Queue of waiting (future) events */
    MprDispatcher   *idleQ;             /**< Queue of idle dispatchers */
    MprDispatcher   *pendingQ;          /**< Queue of pending dispatchers (waiting for resources) */
    MprOsThread     serviceThread;      /**< Thread running the dispatcher service */
    MprTicks        delay;              /**< Maximum sleep time before awaking */
    int             eventCount;         /**< Count of events */
    int             waiting;            /**< Waiting for I/O (sleeping) */
    struct MprCond  *waitCond;          /**< Waiting sync */
    struct MprMutex *mutex;             /**< Multi-thread sync */
} MprEventService;

/**
    Clear the event service waiting flag
    @ingroup MprDispatcher
    @stability Stable
    @internal
 */
PUBLIC void mprClearWaiting(void);

/**
    Create a new event dispatcher.
    @description Dispatchers are event queues that serialize the execution of work. Most of the MPR routines are not thread-safe and thus access to objects needs to be serialized by creating events to run on dispatchers. Resources such as connections will typically own a dispatcher that is used to serialize their work.
    @param name Useful name for debugging
    @param flags Dispatcher flags.
    @returns a Dispatcher object that can manage events and be used with mprCreateEvent
    @ingroup MprDispatcher
    @stability Internal
 */
PUBLIC MprDispatcher *mprCreateDispatcher(cchar *name, int flags);

/**
    Disable a dispatcher from service events. This removes the dispatcher from any dispatcher queues and allows
    it to be garbage collected.
    @param dispatcher Dispatcher to disable.
    @ingroup MprDispatcher
    @stability Internal
 */
PUBLIC void mprDestroyDispatcher(MprDispatcher *dispatcher);

/**
    Get the MPR primary dispatcher
    @returns the MPR dispatcher object
    @ingroup MprDispatcher
    @stability Internal
 */
PUBLIC MprDispatcher *mprGetDispatcher(void);

/*
    mprServiceEvents parameters
 */
#define MPR_SERVICE_NO_BLOCK    0x4         /**< Do not block in mprServiceEvents */
#define MPR_SERVICE_NO_GC       0x8         /**< Don't run GC */

/**
    Service events.
    @description This call services events on all dispatchers and services I/O events.
    An app should dedicate one and only one thread to be an event service thread. That thread should call mprServiceEvents
    from the top-level.
    \n\n
    This call will service events until the timeout expires or if MPR_SERVICE_NO_BLOCK is specified in flags,
    until there are no more events to service. This routine will also return if the MPR has been instructed to terminate and
    is stopping. Calling mprServiceE
    \n\n
    Application event code that is running off a dispatcher should never call mprServiceEvents recursively. Rather, the
    event code should call #mprWaitForEvent if it needs to wait while servicing events on its own dispatcher.
    @param delay Time in milliseconds to wait. Set to zero for no wait. Set to -1 to wait forever.
    @param flags If set to MPR_SERVICE_NO_BLOCK, this call will service all due events without blocking. Otherwise set
        to zero.
    @returns The number of events serviced. Returns MPR_ERR_BUSY is another thread is servicing events.
        Returns when the MPR is stopping or if the timeout expires or if MPR_SERVICE_NO_BLOCK is specified and there are
        no more events to service.
    @ingroup MprDispatcher
    @stability Stable
 */
/*
    Schedule events. An app should dedicate one thread to be an event service thread.
    @param timeout Time in milliseconds to wait. Set to zero for no wait. Set to -1 to wait forever.
    @param flags Set to MPR_SERVICE_NO_BLOCK for non-blocking.
    @returns Number of events serviced.
 */
PUBLIC int mprServiceEvents(MprTicks delay, int flags);

/**
    Set the maximum sleep time for the event service
    @param delay Maximum time to sleep before checking for events to service
    @ingroup MprDispatcher
    @stability Stable
 */
PUBLIC void mprSetEventServiceSleep(MprTicks delay);

/**
    Suspend the current thread
    @description Suspend the current thread until the application is shutting down.
    @param timeout Timeout to wait for shutdown.
    @ingroup MprDispatcher
    @stability Stable
 */
PUBLIC void mprSuspendThread(MprTicks timeout);

/**
    Wait for an event to occur on the given dispatcher
    @description Use this routine to wait for an event and service the event on the given dispatcher.
    This routine should only be called in blocking code.
    \n\n
    This routine yields to the garbage collector by calling #mprYield. Callers must retain all required memory.
    \n\n
    Note that an event may occur before or while invoking this API. To address this window of time, you should
    call #mprGetEventMark to get a Dispatcher event mark and then test your application state to determine if
    waiting is required. If so, then pass the mark to mprWaitForEvent so it can detect
    if any events have been processed since calling mprGetEventMark.
    @param dispatcher Event dispatcher to monitor
    @param timeout for waiting in milliseconds
    @param mark Dispatcher mark returned from #mprGetEventMark
    @return Zero if successful and an event occurred before the timeout expired. Returns #MPR_ERR_TIMEOUT if no event
        is fired before the timeout expires.
    @ingroup MprDispatcher
    @stability Stable
 */
PUBLIC int mprWaitForEvent(MprDispatcher *dispatcher, MprTicks timeout, int64 mark);

/**
    Get an event mark for a dispatcher
    @description An event mark indicates a point in time for a dispatcher. Event marks are incremented for each
    event serviced. This API is used with #mprWaitForEvent to supply an event mark so that mprWaitForEvent can
    detect if any events have been serviced since the mark was taken. This is important so that mprWaitForEvent
    will not miss events that occur before or while invoking #mprWaitForEvent.
    @param dispatcher Event dispatcher
    @return Event mark 64 bit integer
    @ingroup MprDispatcher
    @stability Stable
*/
PUBLIC int64 mprGetEventMark(MprDispatcher *dispatcher);

/**
    Wake the event service
    @description Used to wake the event service if an event is queued for service.
    @ingroup MprDispatcher
    @stability Stable
 */
PUBLIC void mprWakeEventService(void);

/**
    Signal the dispatcher to wakeup and re-examine its queues
    @param dispatcher Event dispatcher to monitor
    @ingroup MprDispatcher
    @stability Internal
    @internal
 */
PUBLIC void mprSignalDispatcher(MprDispatcher *dispatcher);

/**
    Queue an new event for service on a dispatcher.
    @description Create an event to run a callback on an event dispatcher queue.
        The MPR serializes work in a thread-safe manner on dispatcher queues. Resources such as connections will typically
        own a dispatcher that is used to serialize their work.
        \n\n
        This API may be also called by foreign (non-mpr) threads and is the only safe way to invoke MPR services from
        a foreign-thread. The reason for this is that the MPR uses a cooperative garbage collector and a foreign thread
        may call into the MPR at an inopportune time when the MPR is running the garbage collector which requires sole
        access to application memory.
    @param dispatcher Dispatcher object created via mprCreateDispatcher
        Set to NULL for the MPR dispatcher. Use MPR_EVENT_QUICK in the flags to run the event on the events nonBlock
        dispatcher. This should only be used for quick, non-block event callbacks. If using another dispatcher,
        it is essential that the dispatcher not be destroyed while this event is queued or running.
    @param name Static string name of the event
    @param period Time in milliseconds used by continuous events between firing of the event.
    @param proc Function to invoke when the event is run
    @param data Data to associate with the event and stored in event->data. The data must be either an allocated memory
        object or MPR_EVENT_STATIC_DATA must be specified in flags.
    @param flags Flags to modify the behavior of the event. Valid values are: MPR_EVENT_CONTINUOUS to create an event
        which will be automatically rescheduled according to the specified period. Use MPR_EVENT_STATIC_DATA if the
        data argument does not point to a memory object allocated by the Mpr. Include MPR_EVENT_QUICK to execute the event
        without utilizing using a worker thread. This should only be used for quick non-blocking event callbacks.
        \n\n
        When calling this routine from foreign threads, you must use the MPR_EVENT_FOREIGN flag. IN this case the supplied dispatcher will be ignored and the MPR_EVENT_QUICK and MPR_EVENT_STATIC_DATA flags will be implied. Data supplied from foreign threads must be non-mpr memory and must persist until the callback has completed. This typically means the data memory should either be static or be allocated using malloc() before the call and released via free() in the callback.
    @return Returns the event object if successful. This routine may return before or after the even callback has run.
        If MPR_EVENT_FOREIGN is supplied, the return value is always zero.
    @ingroup MprEvent
    @stability Evolving
 */
PUBLIC MprEvent *mprCreateEvent(MprDispatcher *dispatcher, cchar *name, MprTicks period, void *proc, void *data, int flags);

/*
    Queue a new event for service.
    @description Queue an event for service
    @param dispatcher Dispatcher object created via mprCreateDispatcher
    @param event Event object to queue
    @ingroup MprEvent
    @stability Stable
 */
PUBLIC void mprQueueEvent(MprDispatcher *dispatcher, MprEvent *event);

/**
    Remove an event
    @description Remove a queued event. This is useful to remove continuous events from the event queue.
    @param event Event object returned from #mprCreateEvent
    @ingroup MprEvent
    @stability Stable
 */
PUBLIC void mprRemoveEvent(MprEvent *event);

/**
    Stop an event
    @description Stop a continuous event and remove from the queue.
    @param event Event object returned from #mprCreateEvent
    @ingroup MprEvent
    @stability Stable
 */
PUBLIC void mprStopContinuousEvent(MprEvent *event);

/**
    Restart an event
    @description Restart a continuous event after it has been stopped via #mprStopContinuousEvent. This call will
        add the event to the event queue and it will run after the configured event period has expired.
    @param event Event object returned from #mprCreateEvent
    @ingroup MprEvent
    @stability Stable
 */
PUBLIC void mprRestartContinuousEvent(MprEvent *event);

/**
    Enable or disable an event being continous
    @description This call will modify the continuous property for an event.
    @param event Event object returned from #mprCreateEvent
    @param enable Set to 1 to enable continous scheduling of the event
    @ingroup MprEvent
    @stability Stable
 */
PUBLIC void mprEnableContinuousEvent(MprEvent *event, int enable);

/**
    Create a timer event
    @description Create and queue a timer event for service. This is a convenience wrapper to create continuous
        events over the #mprCreateEvent call.
    @param dispatcher Dispatcher object created via #mprCreateDispatcher
    @param name Debug name of the event
    @param proc Function to invoke when the event is run
    @param period Time in milliseconds used by continuous events between firing of the event.
    @param data Data to associate with the event and stored in event->data.
    @param flags Reserved. Must be set to zero.
    @ingroup MprEvent
    @stability Stable
 */
PUBLIC MprEvent *mprCreateTimerEvent(MprDispatcher *dispatcher, cchar *name, MprTicks period, void *proc, void *data,
    int flags);

/**
    Reschedule an event
    @description Reschedule a continuous event by modifying its period.
    @param event Event object returned from #mprCreateEvent
    @param period Time in milliseconds used by continuous events between firing of the event.
    @ingroup MprEvent
    @stability Stable
 */
PUBLIC void mprRescheduleEvent(MprEvent *event, MprTicks period);

/**
    Start a dispatcher by setting it on the run queue
    @description This is used to ensure that all event activity will only happen on the thread that
    calls mprStartDispatcher.
    @param dispatcher Dispatcher object created via #mprCreateDispatcher
    @return Zero if successful, otherwise a negative MPR status code.
    @stability Stable
    @ingroup MprEvent
 */
PUBLIC int mprStartDispatcher(MprDispatcher *dispatcher);

/**
    Stop a dispatcher by removing it from the run queue
    @param dispatcher Dispatcher object created via #mprCreateDispatcher
    @return Zero if successful, otherwise a negative MPR status code.
    @stability Stable
    @ingroup MprEvent
 */
PUBLIC int mprStopDispatcher(MprDispatcher *dispatcher);

/* Internal API */
PUBLIC MprEvent *mprCreateEventQueue(void);
PUBLIC MprEventService *mprCreateEventService(void);
PUBLIC void mprDedicateWorkerToDispatcher(MprDispatcher *dispatcher, struct MprWorker *worker);
PUBLIC void mprDequeueEvent(MprEvent *event);
PUBLIC bool mprDispatcherHasEvents(MprDispatcher *dispatcher);
PUBLIC int mprDispatchersAreIdle(void);
PUBLIC int mprGetEventCount(MprDispatcher *dispatcher);
PUBLIC MprEvent *mprGetNextEvent(MprDispatcher *dispatcher);
PUBLIC MprDispatcher *mprGetNonBlockDispatcher(void);
PUBLIC void mprInitEventQ(MprEvent *q);
PUBLIC void mprQueueTimerEvent(MprDispatcher *dispatcher, MprEvent *event);
PUBLIC void mprReleaseWorkerFromDispatcher(MprDispatcher *dispatcher, struct MprWorker *worker);
PUBLIC void mprScheduleDispatcher(MprDispatcher *dispatcher);
PUBLIC void mprRescheduleDispatcher(MprDispatcher *dispatcher);
PUBLIC void mprSetDispatcherImmediate(MprDispatcher *dispatcher);
PUBLIC void mprStopEventService(void);
PUBLIC void mprWakeDispatchers(void);
PUBLIC void mprWakePendingDispatchers(void);

/*
    Used in testme scripts
 */
PUBLIC void mprSignalCompletion(MprDispatcher *dispatcher);
PUBLIC bool mprWaitForCompletion(MprDispatcher *dispatcher, MprTicks timeout);

/*********************************** XML **************************************/
/*
    XML parser states. The states that are passed to the user handler have "U" appended to the comment.
    The error states (ERR and EOF) must be negative.
 */
#define MPR_XML_ERR                 -1      /**< Error */
#define MPR_XML_EOF                 -2      /**< End of input */
#define MPR_XML_BEGIN               1       /**< Before next tag               */
#define MPR_XML_AFTER_LS            2       /**< Seen "<"                      */
#define MPR_XML_COMMENT             3       /**< Seen "<!--" (usr)        U    */
#define MPR_XML_NEW_ELT             4       /**< Seen "<tag" (usr)        U    */
#define MPR_XML_ATT_NAME            5       /**< Seen "<tag att"               */
#define MPR_XML_ATT_EQ              6       /**< Seen "<tag att" =             */
#define MPR_XML_NEW_ATT             7       /**< Seen "<tag att = "val"   U    */
#define MPR_XML_SOLO_ELT_DEFINED    8       /**< Seen "<tag../>"          U    */
#define MPR_XML_ELT_DEFINED         9       /**< Seen "<tag...>"          U    */
#define MPR_XML_ELT_DATA            10      /**< Seen "<tag>....<"        U    */
#define MPR_XML_END_ELT             11      /**< Seen "<tag>....</tag>"   U    */
#define MPR_XML_PI                  12      /**< Seen "<?processingInst"  U    */
#define MPR_XML_CDATA               13      /**< Seen "<![CDATA["         U    */

/*
    Lex tokens
 */
typedef enum MprXmlToken {
    MPR_XMLTOK_ERR,
    MPR_XMLTOK_TOO_BIG,                     /* Token is too big */
    MPR_XMLTOK_CDATA,
    MPR_XMLTOK_COMMENT,
    MPR_XMLTOK_INSTRUCTIONS,
    MPR_XMLTOK_LS,                          /* "<" -- Opening a tag */
    MPR_XMLTOK_LS_SLASH,                    /* "</" -- Closing a tag */
    MPR_XMLTOK_GR,                          /* ">" -- End of an open tag */
    MPR_XMLTOK_SLASH_GR,                    /* "/>" -- End of a solo tag */
    MPR_XMLTOK_TEXT,
    MPR_XMLTOK_EQ,
    MPR_XMLTOK_EOF,
    MPR_XMLTOK_SPACE
} MprXmlToken;

/**
    XML callback handler
    @param xp XML instance reference
    @param state XML state
    @param tagName Current XML tag
    @param attName Current XML attribute
    @param value Current XML element value
    @ingroup MprXml
    @stability Stable
  */
typedef int (*MprXmlHandler)(struct MprXml *xp, int state, cchar *tagName, cchar* attName, cchar* value);

/**
    XML input stream function
    @param xp XML instance reference
    @param arg to input stream
    @param buf Buffer into which to read data
    @param size Size of buf
    @stability Stable
 */
typedef ssize (*MprXmlInputStream)(struct MprXml *xp, void *arg, char *buf, ssize size);

/**
    Per XML session structure
    @defgroup MprXml MprXml
    @see MprXml MprXmlHandler MprXmlInputStream mprXmlGetErrorMsg mprXmlGetLineNumber mprXmlGetParseArg mprXmlOpen
        mprXmlParse mprXmlSetInputStraem mprXmlSetParseArg mprXmlSetParseHandler
    @stability Internal
 */
typedef struct MprXml {
    MprXmlHandler       handler;            /**< Callback function */
    MprXmlInputStream   readFn;             /**< Read data function */
    MprBuf              *inBuf;             /**< Input data queue */
    MprBuf              *tokBuf;            /**< Parsed token buffer */
    int                 quoteChar;          /**< XdbAtt quote char */
    int                 lineNumber;         /**< Current line no for debug */
    void                *parseArg;          /**< Arg passed to mprXmlParse() */
    void                *inputArg;          /**< Arg for mprXmlSetInputStream() */
    char                *errMsg;            /**< Error message text */
} MprXml;

/**
    Get the XML error message if mprXmlParse fails
    @param xp XML parser instance returned from mprXmlOpen
    @return A descriptive null-terminated string
    @ingroup MprXml
    @stability Stable
 */
PUBLIC cchar *mprXmlGetErrorMsg(MprXml *xp);

/**
    Get the source XML line number.
    @description This call can be used from within the parser callback or when mprXmlParse fails.
    @param xp XML parser instance returned from mprXmlOpen
    @return The line number for the current token or error.
    @ingroup MprXml
    @stability Stable
 */
PUBLIC int mprXmlGetLineNumber(MprXml *xp);

/**
    Get the XML callback argument
    @param xp XML parser instance returned from mprXmlOpen
    @return Argument defined to use for the callback
    @ingroup MprXml
    @stability Stable
 */
PUBLIC void *mprXmlGetParseArg(MprXml *xp);

/**
    Open an XML parser instance.
    @param initialSize Initialize size of XML in-memory token buffer
    @param maxSize Maximum size of XML in-memory token buffer. Set to -1 unlimited.
    @return An XML parser instance
    @ingroup MprXml
    @stability Stable
 */
PUBLIC MprXml *mprXmlOpen(ssize initialSize, ssize maxSize);

/**
    Run the XML parser
    @param xp XML parser instance returned from mprXmlOpen
    @return Zero if successful. Otherwise returns a negative MPR error code.
    @ingroup MprXml
    @stability Stable
 */
PUBLIC int mprXmlParse(MprXml *xp);

/**
    Define the XML parser input stream. This
    @param xp XML parser instance returned from mprXmlOpen
    @param fn Callback function to provide data to the XML parser. The callback is invoked with the signature:
        ssize callbac(MprXml *xp, void *arg, char *buf, ssize size);
    @param arg Callback argument to pass to the
    @ingroup MprXml
    @stability Stable
 */
PUBLIC void mprXmlSetInputStream(MprXml *xp, MprXmlInputStream fn, void *arg);

/**
    Set the XML callback argument
    @param xp XML parser instance returned from mprXmlOpen
    @param parseArg Argument to use for the callback
    @ingroup MprXml
    @stability Stable
 */
PUBLIC void mprXmlSetParseArg(MprXml *xp, void *parseArg);

/**
    Set the XML parser data handle
    @param xp XML parser instance returned from mprXmlOpen
    @param h Arbitrary data to associate with the parser
    @ingroup MprXml
    @stability Stable
 */
PUBLIC void mprXmlSetParserHandler(MprXml *xp, MprXmlHandler h);

/******************************** JSON ****************************************/
/*
    Flags for mprJsonToString
 */
#define MPR_JSON_PRETTY         0x1         /**< Serialize output in a more human readable, multiline "pretty" format */
#define MPR_JSON_QUOTES         0x2         /**< Serialize output quoting keys */
#define MPR_JSON_STRINGS        0x4         /**< Emit all values as quoted strings */

/*
    Data types for obj property values
 */
#define MPR_JSON_OBJ            0x1         /**< The property is an object */
#define MPR_JSON_ARRAY          0x2         /**< The property is an array */
#define MPR_JSON_VALUE          0x4         /**< The property is a value (false|true|null|undefined|regexp|number|string)  */
#define MPR_JSON_FALSE          0x8         /**< The property is false. MPR_JSON_VALUE also set. */
#define MPR_JSON_NULL           0x10        /**< The property is null. MPR_JSON_VALUE also set. */
#define MPR_JSON_NUMBER         0x20        /**< The property is a number. MPR_JSON_VALUE also set. */
#define MPR_JSON_REGEXP         0x40        /**< The property is a regular expression. MPR_JSON_VALUE also set.  */
#define MPR_JSON_STRING         0x80        /**< The property is a string. MPR_JSON_VALUE also set. */
#define MPR_JSON_TRUE           0x100       /**< The property is true. MPR_JSON_VALUE also set. */
#define MPR_JSON_UNDEFINED      0x200       /**< The property is undefined. MPR_JSON_VALUE also set. */
#define MPR_JSON_OBJ_TYPE       0x7         /**< Mask for core type of obj (obj|array|value) */
#define MPR_JSON_DATA_TYPE      0xFF8       /**< Mask for core type of obj (obj|array|value) */

#define MPR_JSON_STATE_EOF      1           /* End of input */
#define MPR_JSON_STATE_ERR      2           /* Some parse error */
#define MPR_JSON_STATE_NAME     3           /* Expecting a name: */
#define MPR_JSON_STATE_VALUE    4           /* Expecting a value */

#define ITERATE_JSON(obj, child, index) \
    index = 0, child = obj ? obj->children: 0; obj && child && index < obj->length; child = child->next, index++

/**
    JSON Object
    @defgroup MprJson MprJson
    @stability Evolving
    @see mprBlendJson mprGetJsonObj mprGetJson mprGetJsonLength mprLoadJson mprParseJson mprSetJsonError
        mprParseJsonEx mprParseJsonInto mprQueryJson mprRemoveJson mprSetJsonObj mprSetJson mprJsonToString mprLogJson
        mprReadJson mprWriteJsonObj mprWriteJson mprWriteJsonObj
 */
typedef struct MprJson {
    cchar           *name;              /**< Property name for this object */
    cchar           *value;             /**< Property value - always strings */
    int             type;               /**< Property type. Object, Array or value */
    int             length;             /**< Number of child properties */
    struct MprJson  *next;              /**< Next sibling */
    struct MprJson  *prev;              /**< Previous sibling */
    struct MprJson  *children;          /**< Children properties */
} MprJson;

/**
    JSON parsing callbacks
    @ingroup MprJson
    @stability Internal
 */
typedef struct MprJsonCallback {
    /**
        Check state callback for JSON deserialization. This function is called at the entry and exit of object levels
        for arrays and objects.
     */
    int (*checkBlock)(struct MprJsonParser *parser, cchar *name, bool leave);

    /**
        MakeObject callback for JSON deserialization. This function is called to construct an object for each level
        in the object tree. Objects will be either arrays or objects.
     */
    MprJson *(*createObj)(struct MprJsonParser *parser, int type);

    /**
        Handle a parse error. This function is called from mprSetJsonError to handle error reporting.
     */
    void (*parseError)(struct MprJsonParser *parser, cchar *msg);

    /**
        Set a property value in an object.
     */
    int (*setValue)(struct MprJsonParser *parser, MprJson *obj, cchar *name, MprJson *child);

    /**
        Pattern matching callback
     */
    bool (*match)(struct MprJsonParser *parser, cchar *str, cchar *pattern);
} MprJsonCallback;


/**
    JSON parser
    @ingroup MprJson
    @stability Internal
 */
typedef struct MprJsonParser {
    cchar           *input;             /* Current input (unmanaged) */
    cchar           *token;             /* Current parse token */
    cchar           *putback;           /* Putback parse token */
    cchar           *errorMsg;          /* Parse error message */
    void            *data;              /* Custom data handle (unmanaged) */
    cchar           *path;              /* Optional JSON filename */
    MprBuf          *buf;               /* Token buffer */
    MprJsonCallback callback;           /* JSON parser callbacks */
    int             tokid;              /* Current tokend ID */
    int             type;               /* Extra type information */
    int             putid;              /* Putback token id */
    int             lineNumber;         /* Current line number in path */
    int             state;              /* Parse state */
    int             tolerant;           /* Tolerant parsing: unquoted names, comma before last property of object */
} MprJsonParser;

/*
    Flags for mprBlendJson
 */
#define MPR_JSON_COMBINE        0x1     /**< Combine properties using '+' '-' '=' '?' prefixes */
#define MPR_JSON_OVERWRITE      0x2     /**< Default to overwrite existing properties '=' */
#define MPR_JSON_APPEND         0x4     /**< Default to append to existing '+' (default) */
#define MPR_JSON_REPLACE        0x8     /**< Replace existing properties '-' */
#define MPR_JSON_CREATE         0x10    /**< Create if not already existing '?' */

/**
    Blend two JSON objects
    @description This performs an N-level deep clone of the source JSON object to be blended into the destination object.
        By default, this add new object properties and overwrite arrays and string values.
        The property combination prefixes: '+', '=', '-' and '?' to append, overwrite, replace and
            conditionally overwrite are supported if the MPR_JSON_COMBINE flag is present.
    @param dest Parsed JSON object. This is the destination object. The "src" object will be blended into this object.
    @param src Source JSON object to blend into dest. Parsed JSON object returned by mprJsonParser.
    @param flags The MPR_JSON_COMBINE flag enables property name prefixes: '+', '=', '-', '?' to append, overwrite,
        replace and and conditionally overwrite key values if not already present. When adding string properties, values
        will be appended using a space separator. Extra spaces will not be removed on replacement.
            \n\n
        Without MPR_JSON_COMBINE or for properties without a prefix, the default is to blend objects by creating new
        properties if not already existing in the destination, and to treat overwrite arrays and strings.
        Use the MPR_JSON_OVERWRITE flag to override the default appending of objects and rather overwrite existing
        properties. Use the MPR_JSON_APPEND flag to override the default of overwriting arrays and strings and rather
        append to existing properties.
    @return Zero if successful.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC int mprBlendJson(MprJson *dest, MprJson *src, int flags);

/**
    Clone a JSON object
    @description This does a deep copy of a JSON object tree. This copies all properties and their sub-properties.
    @return A new JSON object that replices the input object.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprJson *mprCloneJson(MprJson *obj);

/**
    Create a JSON object
    @param type Set JSON object type to MPR_JSON_OBJ for an object, MPR_JSON_ARRAY for an array or MPR_JSON_VALUE
        for a value. Note: all values are stored as strings.
        Additional type information may be ored into the type for: MPR_JSON_NUMBER, MPR_JSON_TRUE, MPR_JSON_FALSE,
        MPR_JSON_NULL, MPR_JSON_UNDEFINED.
    @return JSON object
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprJson *mprCreateJson(int type);

/**
    Create a JSON object value
    @param value String value of the json object.
    @param type Set JSON object type to MPR_JSON_OBJ for an object, MPR_JSON_ARRAY for an array or MPR_JSON_VALUE
        for a value. Note: all values are stored as strings.
        Additional type information may be ored into the type for: MPR_JSON_NUMBER, MPR_JSON_TRUE, MPR_JSON_FALSE,
        MPR_JSON_NULL, MPR_JSON_UNDEFINED.
    @return JSON object
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprJson *mprCreateJsonValue(cchar *value, int type);

/**
    Deserialize a simple JSON string and return a hash of properties
    @param str JSON string. This must be an object with one-level of properties
    @return Hash of property values if successful, otherwise null.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprHash *mprDeserialize(cchar *str);

/**
    Deserialize a simple JSON string into the given hash object
    @param str JSON string. This must be an object with one-level of properties
    @param hash Destination MprHash object
    @return The supplied hash if successful. Otherwise null is returned.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprHash *mprDeserializeInto(cchar *str, MprHash *hash);

/**
    Format a JSON name into and output buffer. This handles quotes and backquotes.
    @param buf MprBuf instance to store the output string
    @param name Json name to format
    @param flags Serialization flags. Supported flags include MPR_JSON_QUOTES to always wrap property names in quotes.
    @return The supplied hash if successful. Otherwise null is returned.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC void mprFormatJsonName(MprBuf *buf, cchar *name, int flags);

/**
    Format a string as a JSON string. This handles quotes and backquotes.
    @param buf MprBuf instance to store the output string
    @param value JSON string value to format
    @return The supplied hash if successful. Otherwise null is returned.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC void mprFormatJsonString(MprBuf *buf, cchar *value);

/**
    Format a value as a simple JSON string. This converts any JSON value to a string representation.
    @param buf MprBuf instance to store the output string
    @param type JSON type to format
    @param value JSON value to format
    @param flags Serialization flags. Supported flags include MPR_JSON_STRINGS to emit values as quoted strings.
    @return The supplied hash if successful. Otherwise null is returned.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC void mprFormatJsonValue(MprBuf *buf, int type, cchar *value, int flags);

/**
    Get a parsed JSON object for a key value
    @param obj Parsed JSON object returned by mprJsonParser
    @param key Property name to search for. This may include ".". For example: "settings.mode".
        See #mprQueryJson for a full description of key formats.
    @return Returns the property value as an object, otherwise NULL if not found or not the correct type.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprJson *mprGetJsonObj(MprJson *obj, cchar *key);

/**
    Get a JSON key and return a string value.
    @description This routine is useful to querying JSON property or object values.
        If the supplied key is an array or object, or matches more than one property, the
        result is a string representation of the array or object.
    @param obj Parsed JSON object returned by mprParseJson
    @param key Property name to search for. This may include ".". For example: "settings.mode".
        See #mprQueryJson for a full description of key formats.
    @return A string representation of the selected properties. If a single property is selected and its
        value is a string, that is returned. If the selected property is an array or object, or it
        matches more than one property, the result is a JSON string representation.
        If nothing is matched, null is returned.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC cchar *mprGetJson(MprJson *obj, cchar *key);

/**
    Get the number of child properties in a JSON object
    @param obj Parsed JSON object returned by mprParseJson
    @return The number of direct dependent child properties
    @ingroup MprJson
    @stability Stable
 */
PUBLIC ssize mprGetJsonLength(MprJson *obj);

/**
    Convert a hash object into a JSON object
    @param hash MprHash object
    @return An MprJson instance
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprJson *mprHashToJson(MprHash *hash);

/**
    Convert a JSON object into a Hash object
    @param json JSON object tree
    @return An MprHash instance
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprHash *mprJsonToHash(MprJson *json);

/**
    Serialize a JSON object into a string
    @description Serializes a top level JSON object created via mprParseJson into a characters string in JSON format.
    @param obj Object returned via #mprParseJson
    @param flags Serialization flags. Supported flags include MPR_JSON_PRETTY for a human-readable multiline format.
    MPR_JSON_QUOTES to wrap property names in quotes. Use MPR_JSON_STRINGS to emit all property values as quoted strings.
    @return Returns a serialized JSON character string.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC char *mprJsonToString(MprJson *obj, int flags);

/**
    Load a JSON object from a filename
    @param path Filename path containing a JSON string to load
    @return JSON object tree
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprJson *mprLoadJson(cchar *path);

/**
    Trace the JSON object to the debug log
    @param level Debug trace level
    @param obj Object to trace
    @param fmt Printf style format and args
    @ingroup MprJson
    @stability Stable
 */
PUBLIC void mprLogJson(int level, MprJson *obj, cchar *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
    Parse a JSON string into an object tree.
    @description Deserializes a JSON string created into an object.
        The top level of the JSON string must be an object, array, string, number or boolean value.
    @param str JSON string to deserialize.
    @return Returns a tree of MprJson objects. Each object represents a level in the JSON input stream.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprJson *mprParseJson(cchar *str);

/**
    Extended JSON parsing from a JSON string into an object tree.
    @description Parses a string into a tree of JSON objects
        This extended deserialization API takes callback functions to control how the object tree is constructed.
        The top level of the JSON string must be an object, array, string, number or boolean value.
    @param str JSON string to deserialize. This is an unmanaged reference. i.e. it will not be marked by the garbage
        collector.
    @param callback Callback functions. This is an instance of the #MprJsonCallback structure.
    @param data Opaque object to pass to the given callbacks. This is an unmanaged reference.
    @param obj Optional object to serialize into.
    @param errorMsg Error message if the string fails to parse.
    @return Returns JSON object tree.
    @ingroup MprJson
    @stability Internal
    @internal
 */
PUBLIC MprJson *mprParseJsonEx(cchar *str, MprJsonCallback *callback, void *data, MprJson *obj, cchar **errorMsg);

/**
    Parse a JSON string into an existing object
    @description Deserializes a JSON string created into an object.
        The top level of the JSON string must be an object, array, string, number or boolean value.
    @param str JSON string to deserialize.
    @param obj JSON object to store parsed properties from str.
    @return Returns the object passed in via "obj". This permits chaining.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprJson *mprParseJsonInto(cchar *str, MprJson *obj);

/**
    Query a JSON object for a property key path and execute the given command.
    @description This query API may be used to get, set or remove property values described by a JSON query key.
    @param obj JSON object to examine. This may be a JSON object, array or string.
    @param key The property key may be a multipart property and may include
        . [] and .. substrings. For example: "settings.mode", "colors[2]", "colors[2:4] and "users..name".
        The "." and "[]" operator reference sub-properties. The ".." elipsis operator spans zero or more
        objects levels.
        \n\n
        Inside the [] operator, you may include an expression to select objects that match the given
        expression. The expression is of the form:
        \n\n
        NAME OP value
        \n\n
        where NAME is the name of the property, OP is ==, !=, <=, >=, ~ or !~. The "~" operator is simple
        string pattern match (contains). Note that [expressions] look ahead and select array elements that have
        matching properties.
        \n\n
        For arrays, to compare the array contents value iself, use "@". This is useful
        to select by array element values. For example: colors[@ == 'red'].
        Use "$" to append an element to an array.
        \n\n
        Examples:
        \n\n
        <pre>
        user.name
        user['name']
        users[2]
        users[2:4]
        users[-4:-1]                //  Range from end of array
        users[name == 'john']
        users[age >= 50]
        users[phone ~ ^206]         //  Starts with 206
        users[$]                    //  Append a new element
        colors[@ != 'red']          //  Array element not 'red'
        people..[name == 'john']    //  Elipsis descends down multiple levels
        </pre>
    @param value If a value is provided, the property described by the key is set to the value.
        If getting property values, or removing, set to NULL.
    @param type Value data type used when setting a value. Set to MPR_JSON_FALSE, MPR_JSON_NULL, MPR_JSON_NUMBER,
        MPR_JSON_STRING, MPR_JSON_TRUE, MPR_JSON_UNDEFINED. Set to zero to sleuth the data type based on the supplied
        value. Note: if the type is zero, numeric values will be set to MPR_JSON_NUMBER and "true", "false", "null"
        and "undefined" will have the corresponding data types.
    @return If getting properties, the selected properties are cloned and returned in a JSON array.
        Note: these are not references into the original properties. If the requested properties are not found
        an empty array is returned. If removing properties, the selected properties are removed and returned
        in the result array without cloning. If the properties to be removed cannot be resolved, null is returned.
        If setting properties, the original object is returned if the properties can be successfully defined. Otherwise,
        null is returned.
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC MprJson *mprQueryJson(MprJson *obj, cchar *key, cchar *value, int type);

/**
    Read a JSON object
    @description This is a low-level simple JSON property lookup routine. This does a one-level property lookup and
    returns the actual JSON object and not a clone. Be careful with this API. Objects returned by this API cannot be
    modified or inserted into another JSON object without corrupting the original JSON object. Use #mprQueryJson or
    mprGetJson to lookup properties and return a clone of the object.
    @param obj Parsed JSON object returned by mprParseJson
    @param name Name of the property to lookup.
    @return The matching JSON object. Returns NULL if a matching property is not found.
        Note this is a reference to the actaul JSON object and not a clone of the object.
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC MprJson *mprReadJsonObj(MprJson *obj, cchar *name);

/**
    Read a JSON property
    @description This is a low-level simple JSON property lookup routine. It does a one-level property lookup.
    Use #mprQueryJson or mprGetJson to lookup properties that are not direct properties at the top level of
    the given object i.e. those that contain ".".
    @param obj Parsed JSON object returned by mprParseJson
    @param name Name of the property to lookup.
    @return The property value as a string. Returns NULL if a matching property is not found.
        Note this is a reference to the actaul JSON property value and not a clone of the value.
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC cchar *mprReadJson(MprJson *obj, cchar *name);

/**
    Read a JSON object by value
    @description This is a low-level simple JSON property lookup routine that searches for a property in the JSON object
    by value. It does a one-level property lookup. Use #mprQueryJson or mprGetJson to lookup properties that are not
    direct properties at the top level of the given object i.e. those that contain ".".
    @param obj Parsed JSON object returned by mprParseJson
    @param value Value to search for.
    @return The JSON object or null if not found.
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC MprJson *mprReadJsonValue(MprJson *obj, cchar *value);

/**
    Remove a property from a JSON object
    @param obj Parsed JSON object returned by mprParseJson
    @param key Property name to remove for. This may include ".". For example: "settings.mode".
        See #mprQueryJson for a full description of key formats.
    @return Returns a JSON object array of all removed properties. Array will be empty if not qualifying
        properties were found and removed.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC MprJson *mprRemoveJson(MprJson *obj, cchar *key);

/**
    Remove a child from a JSON object
    WARNING: do not call this API when traversing the object in question using ITERATE_JSON.
    @param obj Parsed JSON object returned by mprParseJson
    @param child JSON child to remove
    @return The removed child element.
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC MprJson *mprRemoveJsonChild(MprJson *obj, MprJson *child);

/**
    Save a JSON object to a filename
    @param obj Parsed JSON object returned by mprParseJson
    @param path Filename path to contain the saved JSON string
    @param flags Same flags as for #mprJsonToString: MPR_JSON_PRETTY, MPR_JSON_QUOTES, MPR_JSON_STRINGS.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup MprJson
    @stability Stable
 */
PUBLIC int mprSaveJson(MprJson *obj, cchar *path, int flags);

/**
    Serialize a hash of properties as a JSON string
    @param hash Hash of properties to examine
    @param flags Serialization flags. Supported flags include MPR_JSON_PRETTY for a human-readable multiline format.
    MPR_JSON_QUOTES to wrap property names in quotes. Use MPR_JSON_STRINGS to emit all property values as quoted strings.
    @return JSON string
    @ingroup MprJson
    @stability Stable
 */
PUBLIC char *mprSerialize(MprHash *hash, int flags);

/**
    Signal a parse error in the JSON input stream.
    @description JSON callback functions will invoke mprSetJsonError when JSON parse or data semantic errors are
        encountered. This routine may be called by the user JSON parse callback to emit a custom parse error notification.
    @param jp JSON control structure
    @param fmt Printf style format string
    @param ... Printf arguments
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC void mprSetJsonError(MprJsonParser *jp, cchar *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/**
    Update a property in a JSON object
    @description This call takes a multipart property name and will operate at any level of depth in the JSON object.
    @param obj Parsed JSON object returned by mprParseJson
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
        See #mprQueryJson for a full description of key formats.
    @param value Property value to set.
    @return Zero if updated successfully.
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC int mprSetJsonObj(MprJson *obj, cchar *key, MprJson *value);

/**
    Update a key/value in the JSON object with a string value
    @description This call takes a multipart property name and will operate at any level of depth in the JSON object.
        This routine supports the mprQueryJson key syntax.
    @param obj Parsed JSON object returned by mprParseJson
    @param key Property name to add/update. This may include "." and the full mprQueryJson syntax.
        For example: "settings.mode".  See #mprQueryJson for a full description of key formats.
    @param value Character string value.
    @param type Set to MPR_JSON_FALSE, MPR_JSON_NULL, MPR_JSON_NUMBER, MPR_JSON_STRING, MPR_JSON_TRUE, MPR_JSON_UNDEFINED.
    @return Zero if updated successfully.
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC int mprSetJson(MprJson *obj, cchar *key, cchar *value, int type);

/**
    Write a property in a JSON object
    @description This is a low-level update of Json property using simple (non-query) keys.
    @param obj Parsed JSON object returned by mprParseJson
    @param key Property name to add/update.
    @param value Property value to set.
    @return Zero if updated successfully.
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC int mprWriteJsonObj(MprJson *obj, cchar *key, MprJson *value);

/**
    Write a key/value in the JSON object with a string value
    @description This is a low-level update of a Json property using simple (non-query) keys.
    @param obj Parsed JSON object returned by mprParseJson
    @param key Property name to add/update.
    @param value Character string value.
    @param type Set to MPR_JSON_FALSE, MPR_JSON_NULL, MPR_JSON_NUMBER, MPR_JSON_STRING, MPR_JSON_TRUE, MPR_JSON_UNDEFINED.
    @return Zero if updated successfully.
    @ingroup MprJson
    @stability Evolving
 */
PUBLIC int mprWriteJson(MprJson *obj, cchar *key, cchar *value, int type);

/********************************* Threads ************************************/
/**
    Thread service
    @ingroup MprThread
    @stability Internal
 */
typedef struct MprThreadService {
    MprList          *threads;              /**< List of all threads */
    struct MprThread *mainThread;           /**< Main application thread */
    struct MprThread *eventsThread;         /**< Event service thread */
    MprCond          *pauseThreads;         /**< Waiting for threads to yield */
    ssize            stackSize;             /**< Default thread stack size */
} MprThreadService;

/**
    Thread main procedure
    @param arg Argument to the thread main
    @param tp Thread instance reference
    @ingroup MprThread
    @stability Stable
 */
typedef void (*MprThreadProc)(void *arg, struct MprThread *tp);

/*
    Internal
 */
PUBLIC MprThreadService *mprCreateThreadService(void);
PUBLIC void mprStopThreadService(void);

/**
    Thread Service.
    @description The MPR provides a cross-platform thread abstraction above O/S native threads. It supports
        arbitrary thread creation, thread priorities, thread management and thread local storage. By using these
        thread primitives with the locking and synchronization primitives offered by #MprMutex, #MprSpin and
        #MprCond - you can create cross platform multi-threaded applications.
    @see MprThread MprThreadProc MprThreadService mprCreateThread mprGetCurrentOsThread mprGetCurrentThread
        mprGetCurrentThreadName mprGetThreadName mprNeedYield mprResetYield mprStartThread mprYield
    @defgroup MprThread MprThread
    @stability Internal
 */
typedef struct MprThread {
    MprOsThread     osThread;           /**< O/S thread id */
#if ME_WIN_LIKE
    handle          threadHandle;       /**< Threads OS handle for WIN */
    HWND            hwnd;               /**< Window handle */
#endif
    MprThreadProc   entry;              /**< Users thread entry point */
    MprMutex        *mutex;             /**< Multi-thread locking */
    MprCond         *cond;              /**< Multi-thread synchronization */
    void            *data;              /**< Data argument (managed) */
    char            *name;              /**< Name of thead for trace */
    ulong           pid;                /**< Owning process id */
    int             priority;           /**< Current priority */
    ssize           stackSize;          /**< Only VxWorks implements */
#if ME_MPR_ALLOC_STACK
    void            *stackBase;         /**< Base of stack (approx) */
    int             peakStack;          /**< Peak stack usage */
#endif
    bool            isWorker: 1;        /**< Is a worker thread */
    bool            isMain: 1;          /**< Is the main thread */
    bool            stickyYield: 1;     /**< Yielded does not auto-clear after GC */
    bool            yielded: 1;         /**< Thread has yielded to GC */
    bool            noyield: 1;         /**< Do not yield (temporary) */
    bool            waitForSweeper: 1;  /**< Yield untill the GC sweeper is complete */
    int             waiting: 1;         /**< Waiting in mprYield */
} MprThread;


/**
    Thread local data storage
    @stability Internal
    @internal
 */
typedef struct MprThreadLocal {
#if ME_UNIX_LIKE
    pthread_key_t   key;                /**< Data key */
#elif ME_WIN_LIKE
    DWORD           key;
#else
    MprHash         *store;             /**< Thread local data store */
#endif
} MprThreadLocal;


/**
    Create a new thread
    @description MPR threads are usually real O/S threads and can be used with the various locking services (#MprMutex,
        #MprCond, #MprSpin) to enable scalable multithreaded applications.
    @param name Unique name to give the thread
    @param proc Entry point function for the thread. #mprStartThread will invoke this function to start the thread
    @param data Thread private data stored in MprThread.data
    @param stackSize Stack size to use for the thread. On VM based systems, increasing this value, does not
        necessarily incurr a real memory (working-set) increase. Set to zero for a default stack size.
    @returns A MprThread object
    @ingroup MprThread
    @stability Stable
 */
PUBLIC MprThread *mprCreateThread(cchar *name, void *proc, void *data, ssize stackSize);

/**
    Get the O/S thread
    @description Get the O/S thread ID for the currently executing thread.
    @return Returns a platform specific O/S thread ID. On Unix, this is a pthread reference. On other systems it is
        a thread integer value.
    @ingroup MprThread
    @stability Stable
 */
PUBLIC MprOsThread mprGetCurrentOsThread(void);

/**
    Get the currently executing thread.
    @description Get the thread object for the currently executing O/S thread.
    @return Returns a thread object representing the current O/S thread.
    @ingroup MprThread
    @stability Stable
 */
PUBLIC MprThread *mprGetCurrentThread(void);

/**
    Return the name of the current thread
    @returns a static thread name.
    @stability Stable
 */
PUBLIC cchar *mprGetCurrentThreadName(void);

/**
    Get the thread name.
    @description MPR threads are usually real O/S threads and can be used with the various locking services (#MprMutex,
        #MprCond, #MprSpin) to enable scalable multithreaded applications.
    @param thread Thread object returned from #mprCreateThread
    @return Returns a string name for the thread.
    @ingroup MprThread
    @stability Stable
 */
PUBLIC cchar *mprGetThreadName(MprThread *thread);

/**
    Set whether a thread can yield for GC
    @param tp Thread object returned by #mprCreateThread. Set to NULL for the current thread.
    @param on Set to true to enable yielding
    @ingroup MprThread
    @stability Prototype
*/
PUBLIC bool mprSetThreadYield(MprThread *tp, bool on);

/**
    Start a thread
    @description Start a thread previously created via #mprCreateThread. The thread will begin at the entry function
        defined in #mprCreateThread.
    @param thread Thread object returned from #mprCreateThread
    @return Returns zero if successful, otherwise a negative MPR error code.
    @ingroup MprThread
    @stability Stable
 */
PUBLIC int mprStartThread(MprThread *thread);

#define MPR_YIELD_DEFAULT   0x0     /**< mprYield flag if GC is required, yield and wait for mark phase to coplete,
                                         otherwise return without blocking.*/
#define MPR_YIELD_COMPLETE  0x1     /**< mprYield flag to wait until the GC entirely completes including sweep phase */
#define MPR_YIELD_STICKY    0x2     /**< mprYield flag to yield and remain yielded until reset. Does not block */

/**
    Signify to the garbage collector that the thread is ready for garbage collection
    @description This routine informas the garbage collector that the thread has secured all memory that must be
    retained and is now ready for garbage collection. The MPR has a cooperative garbage collector that runs only
    when all threads are ready for collection. Consequently, it is essential that threads "yield" before sleeping
    or blocking.
    \n\n
    Normally, all threads yield automatically when waiting for I/O or otherwise sleeping via standard MPR routines.
    MPR threads tyically yield in their event loops and thread pool idle routines, so threads should not need to
    call mprYield unless calling custom blocking routines or long running routines.
    \n\n
    When calling a blocking routine, you should call mprYield(MPR_YIELD_STICK) to put the thread into a yielded state.
    When the blocking call returns, you should call mprResetYield()
    \n\n
    While yielded, all transient memory must have references from "managed" objects (see mprAlloc) to ensure required
    memory is retained. All other memory will be reclaimed.
    \n\n
    If a thread blocks and does not yield, it will prevent garbage collection and the applications memory size will grow
    unnecessarily.
    @param flags Set to MPR_YIELD_WAIT to wait until the next collection is run. Set to MPR_YIELD_COMPLETE to wait until
    the garbage collection is fully complete including sweep phase. This is not normally required as the sweeper runs
    in parallel with user threads. Set to MPR_YIELD_STICKY to remain in the yielded state. This is useful when sleeping
    or blocking waiting for I/O. #mprResetYield must be called after setting a sticky yield.
    @stability Stable
 */
PUBLIC void mprYield(int flags);

#if DOXYGEN
/**
    Test if a thread should call mprYield
    @description This call tests if a thread should yield to the garbage collector.
    @stability Stable
 */
PUBLIC bool mprNeedYield(void);
#else
#define mprNeedYield() (MPR->heap->mustYield)
#endif

/**
    Reset a sticky yield
    @description This call resets a sticky yield established with #mprYield.
    @stability Stable
 */
PUBLIC void mprResetYield(void);

/*
    Internal APIs
 */
PUBLIC int mprMapMprPriorityToOs(int mprPriority);
PUBLIC int mprMapOsPriorityToMpr(int nativePriority);
PUBLIC void mprSetThreadStackSize(ssize size);
PUBLIC int mprSetThreadData(MprThreadLocal *tls, void *value);
PUBLIC void *mprGetThreadData(MprThreadLocal *tls);
PUBLIC MprThreadLocal *mprCreateThreadLocal(void);

/******************************** I/O Wait ************************************/

#define MPR_READABLE           0x2          /**< Read event mask */
#define MPR_WRITABLE           0x4          /**< Write event mask */

#define MPR_READ_PIPE          0            /* Read side of breakPipe */
#define MPR_WRITE_PIPE         1            /* Write side of breakPipe */

#if ME_WIN_LIKE
typedef long (*MprMsgCallback)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
#endif

/**
    Wait Service
    @ingroup MprWaitHandler
    @stability Internal
 */
typedef struct MprWaitService {
    MprList         *handlers;              /* List of handlers */
    int             needRecall;             /* A handler needs a recall due to buffered data */
    int             wakeRequested;          /* Wakeup of the wait service has been requested */
    MprList         *handlerMap;            /* Map of fds to handlers */
#if ME_EVENT_NOTIFIER == MPR_EVENT_ASYNC
    ATOM            wclass;                 /* Window class */
    HWND            hwnd;                   /* Window handle */
    int             nfd;                    /* Last used entry in the handlerMap array */
    int             fdmax;                  /* Size of the fds array */
    int             socketMessage;          /* Message id for socket events */
    MprMsgCallback  msgCallback;            /* Message handler callback */
#elif ME_EVENT_NOTIFIER == MPR_EVENT_EPOLL
    int             epoll;                  /* Epoll descriptor */
    int             breakFd[2];             /* Event or pipe to wakeup */
#elif ME_EVENT_NOTIFIER == MPR_EVENT_KQUEUE
    int             kq;                     /* Kqueue() return descriptor */
#elif ME_EVENT_NOTIFIER == MPR_EVENT_SELECT
    fd_set          readMask;               /* Current read events mask */
    fd_set          writeMask;              /* Current write events mask */
    int             highestFd;              /* Highest socket in masks + 1 */
    int             breakSock;              /* Socket to wakeup select */
    struct sockaddr_in breakAddress;        /* Address of wakeup socket */
#endif /* EVENT_SELECT */
    MprMutex        *mutex;                 /* General multi-thread sync */
    MprSpin         *spin;                  /* Fast short locking */
} MprWaitService;

/*
    Internal
 */
PUBLIC MprWaitService *mprCreateWaitService(void);
PUBLIC void mprTermOsWait(MprWaitService *ws);
PUBLIC void mprStopWaitService(void);
PUBLIC void mprSetWaitServiceThread(MprWaitService *ws, MprThread *thread);
PUBLIC void mprWakeNotifier(void);
#if MPR_EVENT_ASYNC
    PUBLIC void mprManageAsync(MprWaitService *ws, int flags);
#endif
#if MPR_EVENT_EPOLL
    PUBLIC void mprManageEpoll(MprWaitService *ws, int flags);
#endif
#if MPR_EVENT_KQUEUE
    PUBLIC void mprManageKqueue(MprWaitService *ws, int flags);
#endif
#if MPR_EVENT_SELECT
    PUBLIC void mprManageSelect(MprWaitService *ws, int flags);
#endif
#if ME_WIN_LIKE
    PUBLIC void mprSetWinMsgCallback(MprMsgCallback callback);
    PUBLIC void mprServiceWinIO(MprWaitService *ws, int sockFd, int winMask);
    PUBLIC HWND mprCreateWindow(MprThread *tp);
    PUBLIC ATOM mprCreateWindowClass(cchar *name);
    PUBLIC void mprDestroyWindow(HWND hwnd);
    PUBLIC void mprDestroyWindowClass(ATOM wclass);
    PUBLIC HWND mprGetWindow(bool *created);
    PUBLIC HWND mprSetWindowsThread(MprThread *tp);
#else
    #define mprSetWindowsThread(tp)
#endif

/**
    Wait for I/O.
    @description
    This call waits for any I/O events on wait handlers until the given timeout expires.
    This routine yields to the garbage collector by calling #mprYield. Callers must retain all required memory.
    @param ws Wait service object
    @param timeout Timeout in milliseconds to wait for an event.
    @ingroup MprWaitHandler
    @stability Stable
 */
PUBLIC void mprWaitForIO(MprWaitService *ws, MprTicks timeout);

/**
    Wait for I/O on a file descriptor. No processing of the I/O event is done.
    @description This routine yields to the garbage collector by calling #mprYield. Callers must retain all required memory.
    @param fd File descriptor to examine
    @param mask Mask of events of interest (MPR_READABLE | MPR_WRITABLE)
    @param timeout Timeout in milliseconds to wait for an event.
    @returns A count of events received.
    @ingroup MprWaitHandler
    @stability Stable
 */
PUBLIC int mprWaitForSingleIO(int fd, int mask, MprTicks timeout);

/*
    Handler Flags
 */
#define MPR_WAIT_RECALL_HANDLER     0x1     /**< Wait handler flag to recall the handler asap */
#define MPR_WAIT_NEW_DISPATCHER     0x2     /**< Wait handler flag to create a new dispatcher for each I/O event */
#define MPR_WAIT_IMMEDIATE          0x4     /**< Wait handler flag to immediately service event on same thread */
#define MPR_WAIT_NOT_SOCKET         0x8     /**< I/O file descriptor is not a socket - windows will ignore */

/**
    Wait Handler Service
    @description Wait handlers provide callbacks for when I/O events occur. They provide a wait to service many
        I/O file descriptors without requiring a thread per descriptor.
    @see MprEvent MprWaitHandler mprCreateWaitHandler mprQueueIOEvent mprRecallWaitHandler mprRecallWaitHandlerByFd
        mprDestroyWaitHandler mprWaitOn
    @defgroup MprWaitHandler MprWaitHandler
    @stability Internal
 */
typedef struct MprWaitHandler {
    int             desiredMask;        /**< Mask of desired events */
    int             presentMask;        /**< Mask of current events */
    int             fd;                 /**< O/S File descriptor (sp->sock) */
    int             notifierIndex;      /**< Index for notifier */
    int             flags;              /**< Control flags */
    void            *handlerData;       /**< Argument to pass to proc - managed reference */
    MprEvent        *event;             /**< Event object to process I/O events */
    MprWaitService  *service;           /**< Wait service pointer */
    MprDispatcher   *dispatcher;        /**< Event dispatcher to use for I/O events */
    MprEventProc    proc;               /**< Callback event procedure */
    struct MprWorker *requiredWorker;   /**< Designate the required worker thread to run the callback */
    struct MprThread *thread;           /**< Thread executing the callback, set even if worker is null */
    MprCond         *callbackComplete;  /**< Signalled when a callback is complete */
} MprWaitHandler;


/**
    Create a wait handler
    @description Create a wait handler that will be invoked when I/O of interest occurs on the specified file handle
        The wait handler is registered with the MPR event I/O mechanism.
    @param fd File descriptor
    @param mask Mask of events of interest. This is made by oring MPR_READABLE and MPR_WRITABLE
    @param dispatcher Dispatcher object to use for scheduling the I/O event.
    @param proc Callback function to invoke when an I/O event of interest has occurred.
    @param data Data item to pass to the callback
    @param flags Wait handler flags. Use MPR_WAIT_NEW_DISPATCHER to auto-create a new dispatcher for each I/O event.
    @returns A new wait handler registered with the MPR event mechanism
    @ingroup MprWaitHandler
    @stability Stable
 */
PUBLIC MprWaitHandler *mprCreateWaitHandler(int fd, int mask, MprDispatcher *dispatcher, void *proc, void *data, int flags);

/**
    Destroy a wait handler
    @param wp Wait handler object
    @ingroup MprWaitHandler
    @stability Stable
 */
PUBLIC void mprDestroyWaitHandler(MprWaitHandler *wp);

/**
    Remove a wait handler from the wait service
    @param wp Wait handler object
    @ingroup MprWaitHandler
    @stability Stable
 */
PUBLIC void mprRemoveWaitHandler(MprWaitHandler *wp);

/**
    Queue an IO event for dispatch on the wait handler dispatcher
    @param wp Wait handler created via #mprCreateWaitHandler
    @stability Stable
 */
PUBLIC void mprQueueIOEvent(MprWaitHandler *wp);

/**
    Recall a wait handler
    @description Signal that a wait handler should be recalled at the earliest opportunity. This is useful
        when a protocol stack has buffered data that must be processed regardless of whether more I/O occurs.
    @param wp Wait handler to recall
    @ingroup MprWaitHandler
    @stability Stable
 */
PUBLIC void mprRecallWaitHandler(MprWaitHandler *wp);

/**
    Recall a wait handler by fd
    @description Signal that a wait handler should be recalled at the earliest opportunity. This is useful
        when a protocol stack has buffered data that must be processed regardless of whether more I/O occurs.
    @param fd File descriptor that matches that of a wait handler to recall
    @ingroup MprWaitHandler
    @stability Stable
 */
PUBLIC void mprRecallWaitHandlerByFd(Socket fd);

/**
    Subscribe for desired wait events
    @description Subscribe to the desired wait events for a given wait handler.
    @param wp Wait handler created via #mprCreateWaitHandler
    @param desiredMask Mask of desired events (MPR_READABLE | MPR_WRITABLE)
    @ingroup MprWaitHandler
    @stability Stable
 */
PUBLIC void mprWaitOn(MprWaitHandler *wp, int desiredMask);

/*
   Internal
 */
PUBLIC void mprDoWaitRecall(MprWaitService *ws);

/******************************* Notification *********************************/
/**
    Internal
    @ingroup MprWaitHandler
    @stability Internal
 */
PUBLIC int mprCreateNotifierService(MprWaitService *ws);

/**
    Begin I/O notification services on a wait handler
    @param wp Wait handler associated with the file descriptor
    @param mask Mask of events of interest. This is made by oring MPR_READABLE and MPR_WRITABLE
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup MprWaithHandler
    @stability Internal
 */
PUBLIC int mprNotifyOn(MprWaitHandler *wp, int mask);

/********************************** Sockets ***********************************/
/**
    Socket I/O callback procedure. Proc returns non-zero if the socket has been deleted.
    @ingroup MprSocket
    @stability Stable
 */
typedef int (*MprSocketProc)(void *data, int mask);

/**
    Socket service provider interface.
    @ingroup MprSocket
    @stability Internal
 */
typedef struct MprSocketProvider {
    char    *name;                              /**< Socket provider name */
    void    *data;                              /**< Socket provider private data (unmanaged) */
    void    *managed;                           /**< Socket provider private data managed */

    /**
        Close a socket
        @description Close a socket. If the \a graceful option is true, the socket will first wait for written data to drain
            before doing a graceful close.
        @param socket Socket object returned from #mprCreateSocket
        @param graceful Set to true to do a graceful close. Otherwise, an abortive close will be performed.
        @stability Stable
     */
    void    (*closeSocket)(struct MprSocket *socket, bool graceful);

    /**
        Disconnect a socket by closing its underlying file descriptor.
        This is used to prevent further I/O wait events while still preserving the socket object.
        @param socket Socket object
        @stability Stable
     */
    void    (*disconnectSocket)(struct MprSocket *socket);

    /**
        Flush a socket
        @description Flush any buffered data in a socket. Standard sockets do not use buffering and this call
            will do nothing.  SSL sockets do buffer and calling mprFlushSocket will write pending written data.
        @param socket Socket object returned from #mprCreateSocket
        @return A count of bytes actually written. Return a negative MPR error code on errors.
        @stability Stable
     */
    ssize   (*flushSocket)(struct MprSocket *socket);

    /**
        Read from a socket
        @description Read data from a socket. The read will return with whatever bytes are available. If none and the socket
            is in blocking mode, it will block untill there is some data available or the socket is disconnected.
        @param socket Socket object returned from #mprCreateSocket
        @param buf Pointer to a buffer to hold the read data.
        @param size Size of the buffer.
        @return A count of bytes actually read. Return a negative MPR error code on errors.
        @return Return -1 for EOF and errors. On success, return the number of bytes read. Use  mprIsSocketEof to
            distinguision between EOF and errors.
        @stability Stable
     */
    ssize   (*readSocket)(struct MprSocket *socket, void *buf, ssize size);

    /**
        Write to a socket
        @description Write a block of data to a socket. If the socket is in non-blocking mode (the default), the write
            may return having written less than the required bytes.
        @param socket Socket object returned from #mprCreateSocket
        @param buf Reference to a block to write to the socket
        @param size Length of data to write. This may be less than the requested write length if the socket is in
            non-blocking mode. Will return a negative MPR error code on errors.
        @return A count of bytes actually written. Return a negative MPR error code on errors.
        @stability Stable
     */
    ssize   (*writeSocket)(struct MprSocket *socket, cvoid *buf, ssize size);

    /**
        Upgrade a socket to use SSL/TLS
        @param sp Socket to upgrade
        @param ssl SSL configurations to use. Set to NULL to use the default.
        @param peerName Required peer name in handshake with peer. Used by clients to verify the server hostname.
        @returns Zero if successful, otherwise a negative MPR error code.
        @stability Stable
     */
    int     (*upgradeSocket)(struct MprSocket *socket, struct MprSsl *ssl, cchar *peerName);

    /**
        Get the socket state
        @description Get the socket state as a parseable string description
        @param sp Socket object returned from #mprCreateSocket
        @return The an allocated string
        @stability Stable
     */
    char    *(*socketState)(struct MprSocket *socket);
} MprSocketProvider;

/**
    Callback before binding a socket
    @ingroup MprSocket
    @stability Stable
*/
typedef int (*MprSocketPrebind)(struct MprSocket *sock);


/**
    Mpr socket service class
    @ingroup MprSocket
    @stability Internal
 */
typedef struct MprSocketService {
    MprSocketProvider *standardProvider;        /**< Socket provider for non-SSL connections */
    MprSocketProvider *sslProvider;             /**< Socket provider for SSL connections */
    MprSocketPrebind prebind;                   /**< Prebind callback */
    MprList         *secureSockets;             /**< List of secured (matrixssl) sockets */
    MprMutex        *mutex;                     /**< Multithread locking */
    int             maxAccept;                  /**< Maximum number of accepted client socket connections */
    int             numAccept;                  /**< Count of client socket connections */
    int             hasIPv6;                    /**< System has supoprt for IPv6 */
    int             loaded;                     /**< Provider loaded */
} MprSocketService;

#if DOXYGEN
typedef int Socklen;
#endif

/*
    Internal
 */
PUBLIC MprSocketService *mprCreateSocketService(void);

/**
    Determine if SSL is available
    @returns True if SSL is available
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprHasSecureSockets(void);

/**
    Set the maximum number of accepted client connections that are permissable
    @param max New maximum number of accepted client connections.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC int mprSetMaxSocketAccept(int max);

/*
    Socket close flags
 */
#define MPR_SOCKET_GRACEFUL     1           /**< Do a graceful shutdown */

/*
    Socket event types
 */
#define MPR_SOCKET_READABLE     MPR_READABLE
#define MPR_SOCKET_WRITABLE     MPR_WRITABLE

/*
    Socket Flags
 */
#define MPR_SOCKET_BLOCK            0x1     /**< Use blocking I/O */
#define MPR_SOCKET_BROADCAST        0x2     /**< Broadcast mode */
#define MPR_SOCKET_CLOSED           0x4     /**< MprSocket has been closed */
#define MPR_SOCKET_CONNECTING       0x8     /**< MprSocket is connecting */
#define MPR_SOCKET_DATAGRAM         0x10    /**< Use datagrams */
#define MPR_SOCKET_EOF              0x20    /**< Seen end of file */
#define MPR_SOCKET_LISTENER         0x40    /**< MprSocket is server listener */
#define MPR_SOCKET_NOREUSE          0x80    /**< Don't set SO_REUSEADDR option */
#define MPR_SOCKET_NODELAY          0x100   /**< Disable Nagle algorithm */
#define MPR_SOCKET_THREAD           0x200   /**< Process callbacks on a worker thread */
#define MPR_SOCKET_SERVER           0x400   /**< Socket is on the server-side */
#define MPR_SOCKET_BUFFERED_READ    0x800   /**< Socket has buffered read data (in SSL stack) */
#define MPR_SOCKET_BUFFERED_WRITE   0x1000  /**< Socket has buffered write data (in SSL stack) */
#define MPR_SOCKET_DISCONNECTED     0x4000  /**< The mprDisconnectSocket has been called */
#define MPR_SOCKET_HANDSHAKING      0x8000  /**< Doing an SSL handshake */
#define MPR_SOCKET_CERT_ERROR       0x10000 /**< Error when validating peer certificate */

/**
    Socket Service
    @description The MPR Socket service provides IPv4 and IPv6 capabilities for both client and server endpoints.
    Datagrams, Broadcast and point to point services are supported. The APIs can be used in both blocking and
    non-blocking modes.
    \n\n
    The socket service integrates with the MPR worker thread pool and eventing services. Socket connections can be handled
    by threads from the worker thread pool for scalable, multithreaded applications.
    @stability Stable
    @see MprSocket MprSocketPrebind MprSocketProc MprSocketProvider MprSocketService mprAddSocketHandler
        mprCloseSocket mprConnectSocket mprCreateSocket mprCreateSocketService mprCreateSsl mprCloneSsl
        mprDisconnectSocket mprEnableSocketEvents mprFlushSocket mprGetSocketBlockingMode mprGetSocketError
        mprGetSocketHandle mprGetSocketInfo mprGetSocketPort mprGetSocketState mprHasSecureSockets mprIsSocketEof
        mprIsSocketSecure mprListenOnSocket mprLoadSsl mprParseIp mprReadSocket mprSendFileToSocket mprSetSecureProvider
        mprSetSocketBlockingMode mprSetSocketCallback mprSetSocketEof mprSetSocketNoDelay mprSetSslCaFile mprSetSslCaPath
        mprSetSslCertFile mprSetSslCiphers mprSetSslKeyFile mprSetSslDhFile mprSetSslSslProtocols mprSetSslVerifySslClients
        mprWriteSocket mprWriteSocketString mprWriteSocketVector mprSocketHandshaking mprSocketHasBufferedRead
        mprSocketHasBufferedWrite mprUpgradeSocket
    @defgroup MprSocket MprSocket
    @stability Internal
 */
typedef struct MprSocket {
    MprSocketService *service;          /**< Socket service */
    MprWaitHandler  *handler;           /**< Wait handler */
    char            *acceptIp;          /**< Server addresss that accepted a new connection (actual interface) */
    char            *ip;                /**< Server listen address or remote client address */
    char            *errorMsg;          /**< Connection related error messages */
    int             acceptPort;         /**< Server port doing the listening */
    int             port;               /**< Port to listen or connect on */
    Socket          fd;                 /**< Actual socket file handle */
    int             flags;              /**< Current state flags */
    MprSocketProvider *provider;        /**< Socket implementation provider */
    struct MprSocket *listenSock;       /**< Listening socket */
    void            *sslSocket;         /**< Extended SSL socket state */
    struct MprSsl   *ssl;               /**< Selected SSL configuration */
    cchar           *cipher;            /**< Selected SSL cipher */
    cchar           *session;           /**< SSL session ID (dependent on SSL provider) */
    cchar           *peerName;          /**< Peer common SSL name */
    cchar           *peerCert;          /**< Peer SSL certificate */
    cchar           *peerCertIssuer;    /**< Issuer of peer certificate */
    bool            secured;            /**< SSL Peer verified */
    MprMutex        *mutex;             /**< Multi-thread sync */
    void            *data;              /**< Custom user data (unmanaged) */
} MprSocket;


/**
    Vectored write array
    @stability Internal
 */
typedef struct MprIOVec {
    char            *start;             /**< Start of block to write */
    ssize           len;                /**< Length of block to write */
} MprIOVec;


/**
    Accept an incoming connection
    @param listen Listening server socket
    @returns A new socket connection. Windows can return NULL with error set to EAGAIN.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC MprSocket *mprAcceptSocket(MprSocket *listen);

/**
    Add a wait handler to a socket.
    @description Create a wait handler that will be invoked when I/O of interest occurs on the specified socket.
        The wait handler is registered with the MPR event I/O mechanism.
    @param sp Socket object created via mprCreateSocket
    @param mask Mask of events of interest. This is made by oring MPR_READABLE and MPR_WRITABLE
    @param dispatcher Dispatcher object to use for scheduling the I/O event.
    @param proc Callback function to invoke when an I/O event of interest has occurred.
    @param data Data item to pass to the callback
    @param flags Socket handler flags
    @returns A new wait handler registered with the MPR event mechanism
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC MprWaitHandler *mprAddSocketHandler(MprSocket *sp, int mask, MprDispatcher *dispatcher, void *proc, void *data, int flags);

/**
    Clone a socket object
    @description Create an exact copy of a socket object. On return both socket objects share the same O/S socket handle.
    If the original socket has an SSL configuration, the new socket will share the same SSL configuration object.
    @return A new socket object
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC MprSocket *mprCloneSocket(MprSocket *sp);

/**
    Close a socket
    @description Close a socket. If the \a graceful option is true, the socket will first wait for written data to drain
        before doing a graceful close.
    @param sp Socket object returned from #mprCreateSocket
    @param graceful Set to true to do a graceful close. Otherwise, an abortive close will be performed.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC void mprCloseSocket(MprSocket *sp, bool graceful);

/**
    Connect a client socket
    @description Open a client connection
    @param sp Socket object returned via #mprCreateSocket
    @param ip Host or IP address to connect to.
    @param port TCP/IP port number to connect to.
    @param flags Socket flags may use the following flags ored together:
        @li MPR_SOCKET_BLOCK - to use blocking I/O. The default is non-blocking.
        @li MPR_SOCKET_BROADCAST - Use IPv4 broadcast
        @li MPR_SOCKET_DATAGRAM - Use IPv4 datagrams
        @li MPR_SOCKET_NOREUSE - Set NOREUSE flag on the socket
        @li MPR_SOCKET_NODELAY - Set NODELAY on the socket
        @li MPR_SOCKET_THREAD - Process callbacks on a separate thread.
    @return Zero if the connection is successful. Otherwise a negative MPR error code.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC int mprConnectSocket(MprSocket *sp, cchar *ip, int port, int flags);

/**
    Create a socket
    @description Create a new socket
    @return A new socket object
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC MprSocket *mprCreateSocket(void);

/**
    Disconnect a socket by closing its underlying file descriptor. This is used to prevent further I/O wait events while
    still preserving the socket object.
    @param sp Socket object
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC void mprDisconnectSocket(MprSocket *sp);

/**
    Enable socket events for a socket callback
    @param sp Socket object returned from #mprCreateSocket
    @param mask Mask of events to enable
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC void mprEnableSocketEvents(MprSocket *sp, int mask);

/**
    Flush a socket
    @description Flush any buffered data in a socket. Standard sockets do not use buffering and this call will do nothing.
        SSL sockets do buffer and calling mprFlushSocket will write pending written data.
    @param sp Socket object returned from #mprCreateSocket
    @return A count of bytes actually written. Return a negative MPR error code on errors.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC ssize mprFlushSocket(MprSocket *sp);

/**
    Get the socket blocking mode.
    @description Return the current blocking mode setting.
    @param sp Socket object returned from #mprCreateSocket
    @return True if the socket is in blocking mode. Otherwise false.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprGetSocketBlockingMode(MprSocket *sp);

/**
    Get a socket error code
    @description This will map a Windows socket error code into a posix error code.
    @param sp Socket object returned from #mprCreateSocket
    @return A posix error code.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC int mprGetSocketError(MprSocket *sp);

/**
    Get the socket file descriptor.
    @description Get the file descriptor associated with a socket.
    @param sp Socket object returned from #mprCreateSocket
    @return The Socket file descriptor used by the O/S for the socket.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC Socket mprGetSocketHandle(MprSocket *sp);

/**
    Get the socket for an IP:Port address
    @param ip IP address or hostname
    @param port Port number
    @param family Output parameter to contain the Internet protocol family
    @param protocol Output parameter to contain the Internet TCP/IP protocol
    @param addr Output parameter to contain the sockaddr description of the socket address
    @param addrlen Output parameter to hold the length of the sockaddr object
    @return Zero if the call is successful. Otherwise return a negative MPR error code.
    @ingroup MprSocket
    @stability Stable
  */
PUBLIC int mprGetSocketInfo(cchar *ip, int port, int *family, int *protocol, struct sockaddr **addr, Socklen *addrlen);

/**
    Get the port used by a socket
    @description Get the TCP/IP port number used by the socket.
    @param sp Socket object returned from #mprCreateSocket
    @return The integer TCP/IP port number used by the socket.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC int mprGetSocketPort(MprSocket *sp);

/**
    Get the socket state
    @description Get the socket state as string description in JSON format.
    @param sp Socket object returned from #mprCreateSocket
    @return The an allocated string in JSON format. Returns NULL if the state is not available or supported.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC char *mprGetSocketState(MprSocket *sp);

/**
    has the system got a dual IPv4 + IPv6 network stack
    @return True if the network can listen on IPv4 and IPv6 on a single socket
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprHasDualNetworkStack(void);

/**
    Determine if the system support IPv6
    @return True if the address system supports IPv6 networking.
    @ingroup MprSocket
    @stability Stable
    @internal
 */
PUBLIC bool mprHasIPv6(void);

/**
    Indicate that the application layer has buffered data for the socket.
    @description This is used by SSL and other network stacks that buffer pending data
    @param sp Socket object returned from #mprCreateSocket
    @param len Length of buffered data in bytes
    @param dir Buffer direction. Set to MPR_READABLE for buffered read data and MPR_WRITABLE for buffered write data.
 */
PUBLIC void mprHiddenSocketData(MprSocket *sp, ssize len, int dir);

/**
    Determine if the IP address is an IPv6 address
    @param ip IP address
    @return True if the address is an IPv6 address, otherwise zero.
    @ingroup MprSocket
    @stability Stable
    @internal
 */
PUBLIC bool mprIsIPv6(cchar *ip);

/**
    Determine if the socket is secure
    @description Determine if the socket is using SSL to provide enhanced security.
    @param sp Socket object returned from #mprCreateSocket
    @return True if the socket is using SSL, otherwise zero.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprIsSocketSecure(MprSocket *sp);

/**
    Determine if the socket is using IPv6
    Currently only works for server side addresses.
    @param sp Socket object returned from #mprCreateSocket
    @return True if the socket is using IPv6, otherwise zero.
    @internal
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprIsSocketV6(MprSocket *sp);

/**
    Test if the other end of the socket has been closed.
    @description Determine if the other end of the socket has been closed and the socket is at end-of-file.
    @param sp Socket object returned from #mprCreateSocket
    @return True if the socket is at end-of-file.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprIsSocketEof(MprSocket *sp);

/**
    Listen on a server socket for incoming connections
    @description Open a server socket and listen for client connections.
        If ip is null, then this will listen on both IPv6 and IPv4.
    @param sp Socket object returned via #mprCreateSocket
    @param ip IP address to bind to. Set to 0.0.0.0 to bind to all possible addresses on a given port.
    @param port TCP/IP port number to connect to.
    @param flags Socket flags may use the following flags ored together:
        @li MPR_SOCKET_BLOCK - to use blocking I/O. The default is non-blocking.
        @li MPR_SOCKET_BROADCAST - Use IPv4 broadcast
        @li MPR_SOCKET_DATAGRAM - Use IPv4 datagrams
        @li MPR_SOCKET_NOREUSE - Set NOREUSE flag on the socket
        @li MPR_SOCKET_NODELAY - Set NODELAY on the socket
        @li MPR_SOCKET_THREAD - Process callbacks on a separate thread.
    @return Zero if the connection is successful. Otherwise a negative MPR error code.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC Socket mprListenOnSocket(MprSocket *sp, cchar *ip, int port, int flags);

/**
    Parse an socket address IP address.
    @description This parses a string containing an IP:PORT specification and returns the IP address and port
    components. Handles ipv4 and ipv6 addresses.
    @param address An IP:PORT specification. The :PORT is optional. When an IP address contains an ipv6 port it should be
    written as
        aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii    or
       [aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:iiii]:port
    @param ip Pointer to receive a dynamically allocated IP string.
    @param port Pointer to an integer to receive the port value.
    @param secure Pointer to an integer to receive true if the address requires SSL.
    @param defaultPort The default port number to use if the address does not contain a port
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC int mprParseSocketAddress(cchar *address, cchar **ip, int *port, int *secure, int defaultPort);

/**
    Read from a socket
    @description Read data from a socket. The read will return with whatever bytes are available. If none and the socket
        is in blocking mode, it will block untill there is some data available or the socket is disconnected.
    @param sp Socket object returned from #mprCreateSocket
    @param buf Pointer to a buffer to hold the read data.
    @param size Size of the buffer.
    @return A count of bytes actually read. Return a negative MPR error code on errors.
    @return Return -1 for EOF and errors. On success, return the number of bytes read. Use  mprIsSocketEof to
        distinguision between EOF and errors.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC ssize mprReadSocket(MprSocket *sp, void *buf, ssize size);

/**
    Remove a socket wait handler.
    @description Removes the socket wait handler created via mprAddSocketHandler.
    @param sp Socket object created via mprCreateSocket
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC void mprRemoveSocketHandler(MprSocket *sp);

#if !ME_ROM
/**
    Send a file to a socket
    @description Write the contents of a file to a socket. If the socket is in non-blocking mode (the default), the write
        may return having written less than the required bytes. This API permits the writing of data before and after
        the file contents.
    @param file File to write to the socket
    @param sock Socket object returned from #mprCreateSocket
    @param offset offset within the file from which to read data
    @param bytes Length of file data to write
    @param beforeVec Vector of data to write before the file contents
    @param beforeCount Count of entries in beforeVect
    @param afterVec Vector of data to write after the file contents
    @param afterCount Count of entries in afterCount
    @return A count of bytes actually written. Return a negative MPR error code on errors.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC MprOff mprSendFileToSocket(MprSocket *sock, MprFile *file, MprOff offset, MprOff bytes, MprIOVec *beforeVec,
    int beforeCount, MprIOVec *afterVec, int afterCount);
#endif

/**
    Set the socket blocking mode.
    @description Set the blocking mode for a socket. By default a socket is in non-blocking mode where read / write
        calls will not block.
    @param sp Socket object returned from #mprCreateSocket
    @param on Set to zero to put the socket into non-blocking mode. Set to non-zero to enable blocking mode.
    @return The old blocking mode if successful or a negative MPR error code.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC int mprSetSocketBlockingMode(MprSocket *sp, bool on);

/**
    Set the dispatcher to use for socket events
    @param sp Socket object returned from #mprCreateSocket
    @param dispatcher Dispatcher object reference
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC void mprSetSocketDispatcher(MprSocket *sp, MprDispatcher *dispatcher);

/**
    Set an EOF condition on the socket
    @param sp Socket object returned from #mprCreateSocket
    @param eof Set to true to set an EOF condition. Set to false to clear it.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC void mprSetSocketEof(MprSocket *sp, bool eof);

/**
    Set the socket delay mode.
    @description Set the socket delay behavior (nagle algorithm). By default a socket will partial packet writes
        a little to try to accumulate data and coalesce TCP/IP packages. Setting the delay mode to false may
        result in higher performance for interactive applications.
    @param sp Socket object returned from #mprCreateSocket
    @param on Set to non-zero to put the socket into no delay mode. Set to zero to enable the nagle algorithm.
    @return The old delay mode if successful or a negative MPR error code.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC int mprSetSocketNoDelay(MprSocket *sp, bool on);

/**
    Test if the socket is doing an SSL handshake
    @param sp Socket object returned from #mprCreateSocket
    @return True if the SSL stack is handshaking
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprSocketHandshaking(MprSocket *sp);

/**
    Test if the socket has buffered data.
    @description Use this function to avoid waiting for incoming I/O if data is already buffered.
    @param sp Socket object returned from #mprCreateSocket
    @return True if the socket has pending data to read or write.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprSocketHasBuffered(MprSocket *sp);

/**
    Test if the socket has buffered read data.
    @description Use this function to avoid waiting for incoming I/O if data is already buffered.
    @param sp Socket object returned from #mprCreateSocket
    @return True if the socket has pending read data.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprSocketHasBufferedRead(MprSocket *sp);

/**
    Test if the socket has buffered write data.
    @description Use this function to detect that there is buffer data to write in a SSL stack.
    @param sp Socket object returned from #mprCreateSocket
    @return True if the socket has pending write data.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC bool mprSocketHasBufferedWrite(MprSocket *sp);

/**
    Steal the socket handle
    @description Return the socket handle and set the MprSocket handle to the invalid socket.
    This enables callers to use the O/S socket handle for their own purposes.
    @param sp Socket object returned from #mprCreateSocket
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC Socket mprStealSocketHandle(MprSocket *sp);

/**
    Upgrade a socket to use SSL/TLS
    @param sp Socket to upgrade
    @param ssl SSL configurations to use. Set to NULL to use the default.
    @param peerName Required peer name in handshake with peer. Used by clients to verify the server hostname.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC int mprUpgradeSocket(MprSocket *sp, struct MprSsl *ssl, cchar *peerName);

/**
    Write to a socket
    @description Write a block of data to a socket. If the socket is in non-blocking mode (the default), the write
        may return having written less than the required bytes.
    @param sp Socket object returned from #mprCreateSocket
    @param buf Reference to a block to write to the socket
    @param len Length of data to write. This may be less than the requested write length if the socket is in non-blocking
        mode. Will return a negative MPR error code on errors.
    @return A count of bytes actually written. Return a negative MPR error code on errors and if the socket cannot absorb any
        more data. If the transport is saturated, will return a negative error and mprGetError() returns EAGAIN
        or EWOULDBLOCK.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC ssize mprWriteSocket(MprSocket *sp, cvoid *buf, ssize len);

/**
    Write to a string to a socket
    @description Write a string  to a socket. If the socket is in non-blocking mode (the default), the write
        may return having written less than the required bytes.
    @param sp Socket object returned from #mprCreateSocket
    @param str Null terminated string to write.
    @return A count of bytes actually written. Return a negative MPR error code on errors.
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC ssize mprWriteSocketString(MprSocket *sp, cchar *str);

/**
    Write a vector to a socket
    @description Do scatter/gather I/O by writing a vector of buffers to a socket.
    @param sp Socket object returned from #mprCreateSocket
    @param iovec Vector of data to write before the file contents
    @param count Count of entries in beforeVect
    @return A count of bytes actually written. Return a negative MPR error code on errors and if the socket cannot absorb any
        more data. If the transport is saturated, will return a negative error and mprGetError() returns EAGAIN or EWOULDBLOCK
    @ingroup MprSocket
    @stability Stable
 */
PUBLIC ssize mprWriteSocketVector(MprSocket *sp, MprIOVec *iovec, int count);

/************************************ SSL *************************************/
/*
    Root certificates for verifying peer certs.
 */
#ifndef ME_SSL_ROOTS_CERT
    #define ME_SSL_ROOTS_CERT "roots.crt"
#endif
#ifndef ME_MPR_SSL_CACHE
    #define ME_MPR_SSL_CACHE 512
#endif
#ifndef ME_MPR_SSL_LOG_LEVEL
    #define ME_MPR_SSL_LOG_LEVEL 6
#endif
#ifndef ME_MPR_SSL_RENEGOTIATE
    #define ME_MPR_SSL_RENEGOTIATE 1
#endif
#ifndef ME_MPR_SSL_TICKET
    #define ME_MPR_SSL_TICKET 1
#endif
#ifndef ME_MPR_SSL_TIMEOUT
    #define ME_MPR_SSL_TIMEOUT 86400
#endif
#define ME_MPR_HAS_ALPN 1

/**
    Callback function for SNI connections.
    @ingroup MprSsl
    @stability Evolving
 */
typedef struct MprSsl *(*MprMatchSsl)(MprSocket *sp, cchar *hostname);

/**
    SSL control structure
    @defgroup MprSsl MprSsl
    @stability Internal
 */
typedef struct MprSsl {
    cchar           *keyFile;           /**< Alternatively, locate the key in a file */
    cchar           *certFile;          /**< Certificate filename */
    cchar           *revoke;            /**< Certificate revocation list */
    cchar           *caFile;            /**< Certificate verification cert file or bundle */
    cchar           *caPath;            /**< Certificate verification cert directory (OpenSSL only) */
    cchar           *ciphers;           /**< Candidate ciphers to use */
    cchar           *hostname;          /**< Hostname when using SNI */
    MprList         *alpn;              /**< ALPN protocols */
    void            *config;            /**< Extended provider SSL configuration */
    bool            changed;            /**< Set if there is a change in the SSL config. Reset by providers */
    bool            configured;         /**< Set if this SSL configuration has been processed */
    bool            ticket;             /**< Enable session tickets */
    bool            renegotiate;        /**< Renegotiate sessions */
    bool            verifyPeer;         /**< Verify the peer verificate */
    bool            verifyIssuer;       /**< Set if the certificate issuer should be also verified */
    bool            verified;           /**< Peer has been verified */
    int             logLevel;           /**< Level at which to start tracing SSL events */
    int             verifyDepth;        /**< Cert chain depth that should be verified */
    int             protocols;          /**< SSL protocols */
    MprMatchSsl     matchSsl;           /**< Match the SSL configuration for SNI */
    MprMutex        *mutex;             /**< Multithread sync */
} MprSsl;

/*
    SSL protocols
 */
#define MPR_PROTO_SSLV2    0x1              /**< SSL V2 protocol */
#define MPR_PROTO_SSLV3    0x2              /**< SSL V3 protocol */
#define MPR_PROTO_TLSV1_0  0x8              /**< TLS V1.0 protocol */
#define MPR_PROTO_TLSV1_1  0x10             /**< TLS V1.1 protocol */
#define MPR_PROTO_TLSV1_2  0x20             /**< TLS V1.2 protocol */
#define MPR_PROTO_TLSV1    (MPR_PROTO_TLSV1_1 | MPR_PROTO_TLSV1_2)
#define MPR_PROTO_ALL      0x2F             /**< All protocols */

/**
    Add the ciphers to use for SSL
    @param ssl SSL instance returned from #mprCreateSsl
    @param ciphers Cipher string to add to any existing ciphers
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprAddSslCiphers(struct MprSsl *ssl, cchar *ciphers);

/**
    Create the SSL control structure
    @param server True if the SSL configuration will be used on the server side.
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC struct MprSsl *mprCreateSsl(int server);

/**
    Create the a new SSL control structure based on an existing structure
    @param src Structure to clone
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC struct MprSsl *mprCloneSsl(MprSsl *src);

/**
    Load the SSL module.
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC int mprLoadSsl(void);

/**
    Initialize the SSL provider
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC int mprSslInit(void *unused, MprModule *module);

/**
    Set the ALPN protocols for SSL
    @ingroup MprSsl
    @stability Prototype
 */
PUBLIC void mprSetSslAlpn(struct MprSsl *ssl, cchar *protocols);

/**
    Set certificate to use for SSL
    @param ssl SSL instance returned from #mprCreateSsl
    @param certFile Path to the SSL certificate file
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprSetSslCertFile(struct MprSsl *ssl, cchar *certFile);

/**
    Set the client certificate file to use for SSL
    @param ssl SSL instance returned from #mprCreateSsl
    @param caFile Path to the SSL client certificate file
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprSetSslCaFile(struct MprSsl *ssl, cchar *caFile);

/**
    Set the path for the client certificate directory
    @description This is supported for OpenSSL only.
    @param ssl SSL instance returned from #mprCreateSsl
    @param caPath Path to the SSL client certificate directory
    @ingroup MprSsl
    @stability Deprecated
    @internal
 */
PUBLIC void mprSetSslCaPath(struct MprSsl *ssl, cchar *caPath);

/**
    Set the ciphers to use
    @param ssl SSL instance returned from #mprCreateSsl
    @param ciphers String of suitable ciphers
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprSetSslCiphers(MprSsl *ssl, cchar *ciphers);

/**
    Set the key file to use for SSL
    @param ssl SSL instance returned from #mprCreateSsl
    @param keyFile Path to the SSL key file
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprSetSslKeyFile(struct MprSsl *ssl, cchar *keyFile);

/**
    Set the desired hostname for this SSL configuration when using SNI
    @param ssl SSL instance returned from #mprCreateSsl
    @param hostname Name of the host when using SNI
    @ingroup MprSsl
    @stability Evolving
 */
PUBLIC void mprSetSslHostname(MprSsl *ssl, cchar *hostname);

/**
    Set the SSL log level at which to start tracing SSL events
    @param ssl SSL instance returned from #mprCreateSsl
    @param level Log level (0-9)
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprSetSslLogLevel(struct MprSsl *ssl, int level);

/**
    Set a match callback to select the appropriate SSL configuration to use in response to a client SNI hello.
    @param ssl SSL configuration instance
    @param match MprMatchSsl callback.
    @ingroup MprSsl
    @stability Evolving
 */
PUBLIC void mprSetSslMatch(struct MprSsl *ssl, MprMatchSsl match);

/**
    Set the SSL protocol to use
    @param ssl SSL instance returned from #mprCreateSsl
    @param protocols SSL protocols mask
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprSetSslProtocols(struct MprSsl *ssl, int protocols);

/**
    Set the SSL provider to use
    @param provider Socket provider object
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprSetSslProvider(MprSocketProvider *provider);

/**
    Control SSL session renegotiation
    @param ssl SSL instance returned from #mprCreateSsl
    @param enable Set to true to enable renegotiation (enabled by default)
    @ingroup MprSsl
    @stability Internal
 */
PUBLIC void mprSetSslRenegotiate(MprSsl *ssl, bool enable);

/**
    Define a list of certificates to revoke
    @param ssl SSL instance returned from #mprCreateSsl
    @param revoke Path to the SSL certificate revocation list
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprSetSslRevoke(struct MprSsl *ssl, cchar *revoke);

/**
    Enable SSL session tickets
    @param ssl SSL instance returned from #mprCreateSsl
    @param enable Set to true to enable
    @ingroup MprSsl
    @stability Stable
*/
PUBLIC void mprSetSslTicket(MprSsl *ssl, bool enable);

/**
    Control the depth of SSL SSL certificate verification
    @param ssl SSL instance returned from #mprCreateSsl
    @param depth Set to the number of intermediate certificates to verify. Defaults to 1.
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprVerifySslDepth(struct MprSsl *ssl, int depth);

/**
    Control the verification of SSL certificate issuers
    @param ssl SSL instance returned from #mprCreateSsl
    @param on Set to true to enable SSL certificate issuer verification.
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprVerifySslIssuer(struct MprSsl *ssl, bool on);

/**
    Require verification of peer certificates
    @param ssl SSL instance returned from #mprCreateSsl
    @param on Set to true to enable peer SSL certificate verification.
    @ingroup MprSsl
    @stability Stable
 */
PUBLIC void mprVerifySslPeer(struct MprSsl *ssl, bool on);

#if ME_COM_EST
    PUBLIC int mprCreateEstModule(void);
#endif
#if ME_COM_MATRIXSSL
    PUBLIC int mprCreateMatrixSslModule(void);
#endif
#if ME_COM_NANOSSL
    PUBLIC int mprCreateNanoSslModule(void);
#endif
#if ME_COM_OPENSSL
    PUBLIC int mprCreateOpenSslModule(void);
#endif

/******************************* Worker Threads *******************************/
/**
    Worker thread callback signature
    @param data worker callback data. Set via mprStartWorker or mprActivateWorker
    @param worker Reference to the worker thread object
    @ingroup MprWorker
    @stability Stable
 */
typedef void (*MprWorkerProc)(void *data, struct MprWorker *worker);

/**
    Statistics for Workers
    @ingroup MprWorker
    @stability Internal
 */
typedef struct MprWorkerStats {
    int     max;            /**< Configured max number of workers */
    int     min;            /**< Configured minimum number of workers */
    int     maxUsed;        /**< Max number of workers ever used used */
    int     idle;           /**< Number of idle workers */
    int     busy;           /**< Number of busy workers */
    int     yielded;        /**< Number of busy workers yielded for GC */
} MprWorkerStats;

/**
    Get the Worker service statistics
    @param stats Reference to stats object to receive the stats
    @ingroup MprWorker
    @stability Internal
 */
PUBLIC void mprGetWorkerStats(MprWorkerStats *stats);

/**
    Worker Thread Service
    @description The MPR provides a worker thread pool for rapid starting and assignment of threads to tasks.
    @ingroup MprWorker
    @stability Internal
 */
typedef struct MprWorkerService {
    MprList         *busyThreads;       /**< List of threads to service tasks */
    MprList         *idleThreads;       /**< List of threads to service tasks */
    int             maxThreads;         /**< Max # threads in worker pool */
    int             maxUsedThreads;     /**< Max threads ever used */
    int             minThreads;         /**< Max # threads in worker pool */
    int             nextThreadNum;      /**< Unique next thread number */
    int             numThreads;         /**< Current number of threads in worker pool */
    ssize           stackSize;          /**< Stack size for worker threads */
    MprMutex        *mutex;             /**< Per task synchronization */
    struct MprEvent *pruneTimer;        /**< Timer for excess threads pruner */
    MprWorkerProc   startWorker;        /**< Worker thread startup hook */
} MprWorkerService;


/*
    Internal
 */
PUBLIC MprWorkerService *mprCreateWorkerService(void);
PUBLIC int mprStartWorkerService(void);
PUBLIC void mprStopWorkers(void);
PUBLIC void mprSetWorkerStartCallback(MprWorkerProc start);

/**
    Get the count of available worker threads
    Return the count of free threads in the worker thread pool.
    @returns An integer count of worker threads.
    @ingroup MprWorker
    @stability Stable
 */
PUBLIC int mprAvailableWorkers(void);

/**
    Set the default worker stack size
    @param size Stack size in bytes
    @ingroup MprWorker
    @stability Stable
 */
PUBLIC void mprSetWorkerStackSize(int size);

/**
    Set the minimum count of worker threads
    Set the count of threads the worker pool will have. This will cause the worker pool to pre-create at least this
    many threads.
    @param count Minimum count of threads to use.
    @ingroup MprWorker
    @stability Stable
 */
PUBLIC void mprSetMinWorkers(int count);

/**
    Set the maximum count of worker threads
    Set the maximum number of worker pool threads for the MPR. If this number if less than the current number of threads,
        excess threads will be gracefully pruned as they exit.
    @param count Maximum limit of threads to define.
    @ingroup MprWorker
    @stability Stable
 */
PUBLIC void mprSetMaxWorkers(int count);

/**
    Get the maximum count of worker pool threads
    Get the maximum limit of worker pool threads.
    @return The maximum count of worker pool threads.
    @ingroup MprWorker
    @stability Stable
 */
PUBLIC int mprGetMaxWorkers(void);

/*
    Worker Thread State
 */
#define MPR_WORKER_BUSY        0x1          /**< Worker currently running to a callback */
#define MPR_WORKER_PRUNED      0x2          /**< Worker has been pruned and will be terminated */
#define MPR_WORKER_IDLE        0x4          /**< Worker is sleeping (idle) on idleCond */

/**
    Worker thread structure. Worker threads are allocated and dedicated to tasks. When idle, they are stored in
    an idle worker pool. An idle worker pruner runs regularly and terminates idle workers to save memory.
    @defgroup MprWorker MprWorker
    @see MPrWorkerProc MprWorkerService MprWorkerStats mprActivateWorker mprDedicateWorker mprGetCurrentWorker
        mprGetMaxWorkers mprGetWorkerServiceStats mprReleaseWorker mprSetMaxWorkers mprSetMinWorkers
        mprSetWorkerStackSize mprStartWorker
    @stability Internal
 */
typedef struct MprWorker {
    MprWorkerProc   proc;                   /**< Procedure to run */
    MprWorkerProc   cleanup;                /**< Procedure to cleanup after run before sleeping */
    void            *data;                  /**< User per-worker data */
    int             state;                  /**< Worker state */
    int             running;                /**< Worker running a job */
    MprThread       *thread;                /**< Thread associated with this worker */
    MprTicks        lastActivity;           /**< When the worker was last used */
    MprWorkerService *workerService;        /**< Worker service */
    MprCond         *idleCond;              /**< Used to wait for work */
} MprWorker;

/*
    Internal
 */
PUBLIC void mprActivateWorker(MprWorker *worker, MprWorkerProc proc, void *data);

/**
    Dedicate a worker thread to a current real thread. This implements thread affinity and is required on some platforms
        where some APIs (waitpid on uClibc) cannot be called on a different thread.
    @param worker Worker object
    @param worker Worker thread reference
    @ingroup MprWorker
    @stability Internal
 */
PUBLIC void mprDedicateWorker(MprWorker *worker);

/**
    Get the worker object if the current thread is actually a worker thread.
    @returns A worker thread object if the thread is a worker thread. Otherwise, NULL.
    @ingroup MprWorker
    @stability Stable
 */
PUBLIC MprWorker *mprGetCurrentWorker(void);

/**
    Get the count of workers in the busy queue.
    @description This is thread-safe with respect to MPR->state
    @return Count of workers in the busy queue
    @ingroup MprWorker
    @stability Stable
 */
PUBLIC ssize mprGetBusyWorkerCount(void);

/**
    Release a worker thread. This releases a worker thread to be assignable to any real thread.
    @param worker Worker object
    @param worker Worker thread reference
    @stability Internal
 */
PUBLIC void mprReleaseWorker(MprWorker *worker);

/**
    Start a worker thread
    @description Start a worker thread executing the given worker procedure callback.
    @param proc Worker procedure callback
    @param data Data parameter to the callback
    @returns Zero if successful, otherwise a negative MPR error code.
    @stability Internal
 */
PUBLIC int mprStartWorker(MprWorkerProc proc, void *data);

/********************************** Crypto ************************************/
/**
    Return a random number
    @returns A random integer
    @ingroup Mpr
    @stability Stable
 */
PUBLIC int mprRandom(void);

/**
    Decode a null terminated string using base-46 encoding.
    @description Decoding will terminate at the first null or '='.
    @param str String to decode
    @returns Buffer containing the encoded data
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprDecode64(cchar *str);

/**
    Decode base 64 blocks up to a NULL or equals
 */
#define MPR_DECODE_TOKEQ 1

/**
    Decode a null terminated string  using base-46 encoding.
    @param buf String to decode
    @param len Return parameter with the Length of the decoded data
    @param flags Set to MPR_DECODE_TOKEQ to stop at the first '='
    @returns Buffer containing the encoded data and returns length in len.
    @ingroup Mpr
    @stability Stable
  */
PUBLIC char *mprDecode64Block(cchar *buf, ssize *len, int flags);

/**
    Encode a string using base-46 encoding.
    @param str String to encode
    @returns Buffer containing the encoded string.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprEncode64(cchar *str);

/**
    Encode buffer using base-46 encoding.
    @param buf Buffer to encode
    @param len Length of the buffer to encode
    @returns Buffer containing the encoded string.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprEncode64Block(cchar *buf, ssize len);

/**
    Get an MD5 checksum
    @param str String to examine
    @returns An allocated MD5 checksum string.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprGetMD5(cchar *str);

/**
    Get an MD5 checksum with optional prefix string and buffer length
    @param buf Buffer to checksum
    @param len Size of the buffer
    @param prefix String prefix to insert at the start of the result
    @returns An allocated MD5 checksum string.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprGetMD5WithPrefix(cchar *buf, ssize len, cchar *prefix);

/**
    Get an SHA1 checksum
    @param str String to examine
    @returns An allocated SHA1 checksum string.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprGetSHA(cchar *str);

/**
    Get an SHA1 checksum with optional prefix string and buffer length
    @param buf Buffer to checksum
    @param len Size of the buffer
    @param prefix String prefix to insert at the start of the result
    @returns An allocated string containing an SHA1 checksum.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprGetSHAWithPrefix(cchar *buf, ssize len, cchar *prefix);

/**
    Get an SHA1 checksum of a null terminated string
    @param str String to checksum
    @returns An allocated string containing an SHA1 checksum.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprGetSHABase64(cchar *str);

/**
    Encrypt a password using the Blowfish algorithm
    @param password User's password to encrypt
    @param salt Salt text to add to password. Helps to make each user's password unique.
    @param rounds Number of times to encrypt. More times, makes the routine slower and passwords harder to crack.
    @return The encrypted password.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprCryptPassword(cchar *password, cchar *salt, int rounds);

/**
    Get a password from the terminal console
    @param prompt Text prompt to display before reading the password
    @return The entered password.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprGetPassword(cchar *prompt);

/**
    Make salt for adding to a password.
    @param size Size in bytes of the salt text.
    @return The random salt text.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprMakeSalt(ssize size);

/**
    Make a password hash for a plain-text password using the Blowfish algorithm.
    @param password User's password to encrypt
    @param saltLength Length of salt text to add to password. Helps to make each user's password unique.
    @param rounds Number of times to encrypt. More times, makes the routine slower and passwords harder to crack.
    @return The encrypted password.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprMakePassword(cchar *password, int saltLength, int rounds);

/**
    Check a plain-text password against the defined hashed password.
    @param plainTextPassword User's plain-text-password to check
    @param passwordHash Required password in hashed format previously computed by mprMakePassword.
    @return True if the password is correct.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC bool mprCheckPassword(cchar *plainTextPassword, cchar *passwordHash);

/********************************* Encoding ***********************************/
/*
    Character encoding masks
 */
#define MPR_ENCODE_HTML             0x1
#define MPR_ENCODE_SHELL            0x2
#define MPR_ENCODE_URI              0x4             /* Encode for ejs Uri.encode */
#define MPR_ENCODE_URI_COMPONENT    0x8             /* Encode for ejs Uri.encodeComponent */
#define MPR_ENCODE_JS_URI           0x10            /* Encode according to ECMA encodeUri() */
#define MPR_ENCODE_JS_URI_COMPONENT 0x20            /* Encode according to ECMA encodeUriComponent */
#define MPR_ENCODE_SQL              0x40            /* Encode for a SQL command */

/**
    Encode a string escaping typical command (shell) characters
    @description Encode a string escaping all dangerous characters that have meaning for the unix or MS-DOS command shells.
    @param cmd Command string to encode
    @param escChar Escape character to use when encoding the command.
    @return An allocated string containing the escaped command.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprEscapeCmd(cchar *cmd, int escChar);

/**
    Encode a string by escaping typical HTML characters
    @description Encode a string escaping all dangerous characters that have meaning in HTML documents
    @param html HTML content to encode
    @return An allocated string containing the escaped HTML.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprEscapeHtml(cchar *html);

/**
    Encode a string by escaping SQL special characters
    @description Encode a string escaping all dangerous characters that have meaning in SQL commands
    @param cmd SQL command to encode
    @return An allocated string containing the escaped SQL command.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprEscapeSQL(cchar *cmd);

/**
    Encode a string by escaping URI characters
    @description Encode a string escaping all characters that have meaning for URIs.
    @param uri URI to encode
    @param map Map to encode characters. Select from MPR_ENCODE_URI or MPR_ENCODE_URI_COMPONENT.
    @return An allocated string containing the encoded URI.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprUriEncode(cchar *uri, int map);

/**
    Decode a URI string by de-scaping URI characters
    @description Decode a string with www-encoded characters that have meaning for URIs.
    @param uri URI to decode
    @return A reference to the buf argument.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprUriDecode(cchar *uri);

/**
    Decode a URI string by de-scaping URI characters
    @description Decode a string with www-encoded characters that have meaning for URIs.
        This routines operates in-situ and modifies the buffer.
    @param uri URI to decode
    @return A reference to the buf argument.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC char *mprUriDecodeInSitu(char *uri);

/********************************* Signals ************************************/

#if MACOSX
    #define MPR_MAX_SIGNALS 40              /**< Max signals that can be managed */
#elif LINUX
    #define MPR_MAX_SIGNALS 48
#else
    #define MPR_MAX_SIGNALS 40
#endif

/**
    Signal callback procedure
    @ingroup MprSignal
    @stability Stable
 */
typedef void (*MprSignalProc)(void *arg, struct MprSignal *sp);


/**
    Per signal structure
    @ingroup MprSignal
    @stability Internal
 */
typedef struct MprSignalInfo {
    int             triggered;              /**< Set to true when triggered */
} MprSignalInfo;


/**
    Signal control structure
    @defgroup MprSignal MprSignal
    @see MprSignalProc MprSignalService MprSingalInfo mprAddSignalHandler mprAddStandardSignals
    @stability Internal
 */
typedef struct MprSignal {
    struct MprSignal *next;                                 /**< Chain of handlers on the same signo */
    MprSignalProc   handler;                                /**< Signal handler (non-native) */
    void            (*sigaction)(int, siginfo_t*, void *);  /**< Prior sigaction handler */
    void            *data;                                  /**< Handler data */
    MprDispatcher   *dispatcher;                            /**< Dispatcher to service handler */
    int             flags;                                  /**< Control flags */
    int             signo;                                  /**< Signal number */
} MprSignal;


/**
    Signal service control
    @ingroup MprSignal
    @stability Internal
 */
typedef struct MprSignalService {
    MprSignal       **signals;              /**< Signal handlers */
    MprList         *standard;              /**< Standard signal handlers */
    MprMutex        *mutex;                 /**< Multithread sync */
    MprSignalInfo   info[MPR_MAX_SIGNALS];  /**< Actual signal info and arg */
    int             hasSignals;             /**< Signal sent to process */
#if ME_UNIX_LIKE
    struct sigaction prior[MPR_MAX_SIGNALS];/**< Prior sigaction handler before hooking */
#endif
} MprSignalService;


/*
    Internal
 */
PUBLIC MprSignalService *mprCreateSignalService(void);
PUBLIC void mprStopSignalService(void);
PUBLIC void mprRemoveSignalHandler(MprSignal *sp);
PUBLIC void mprServiceSignals(void);

/**
    Add standard trapping of system signals. The trapped signals are SIGINT, SIGQUIT, SIGTERM, SIGPIPE and SIGXFSZ.
    SIGPIPE and SIGXFSZ are ignored. A shutdown is initiated for SIGTERM whereas SIGINT and SIGQUIT will
    do an abortive exit. SIGUSR1 will do an in-process restart.
    @ingroup MprSignal
    @stability Stable
 */
PUBLIC void mprAddStandardSignals(void);

#define MPR_SIGNAL_BEFORE   0x1             /**< Flag to mprAddSignalHandler to run handler before existing handlers */
#define MPR_SIGNAL_AFTER    0x2             /**< Flag to mprAddSignalHandler to run handler after existing handlers */

/**
    Add a signal handler. The signal handling mechanism will trap the specified signal if issued and create an
    event on the given dispatcher. This will cause the handler function to be safely run by the dispatcher.
    Normally, signal handlers are difficult to write as the code must be Async-safe. This API permits the use of
    common, single-threaded code to be used for signal handlers without worrying about pre-emption by other signals
    or threads.
    @param signo Signal number to handle
    @param handler Call back procedure to invoke. This has the signature #MprSignalProc.
    @param arg Argument to provide to the handler.
    @param dispatcher Event dispatcher on which to queue an event to run the handler.
    @param flags Set to either MPR_SIGNAL_BEFORE or MPR_SIGNAL_AFTER to run the handler before/after existing handlers.
    @ingroup MprSignal
    @stability Stable
 */
PUBLIC MprSignal *mprAddSignalHandler(int signo, void *handler, void *arg, MprDispatcher *dispatcher, int flags);

/******************************** Commands ************************************/
/**
    Callback function before doing a fork()
    @ingroup MprCmd
    @stability Stable
 */
typedef void (*MprForkCallback)(void *arg);

/**
    Command execution service
    @stability Internal
 */
typedef struct MprCmdService {
    MprList         *cmds;          /* List of all commands. This is a static list and elements are not retained for GC */
    MprMutex        *mutex;         /* Multithread sync */
} MprCmdService;

/*
    Internal
 */
PUBLIC MprCmdService *mprCreateCmdService(void);
PUBLIC void mprStopCmdService(void);

#define MPR_CMD_EOF_COUNT       2
#define MPR_CMD_VXWORKS_EOF     "_ _EOF_ _"     /**< Special string for VxWorks CGI to emit to signal EOF */
#define MPR_CMD_VXWORKS_EOF_LEN 9               /**< Length of MPR_CMD_VXWORKS_EOF */

/*
    Channels for clientFd and serverFd
 */
#define MPR_CMD_STDIN           0       /**< Stdout for the client side */
#define MPR_CMD_STDOUT          1       /**< Stdin for the client side */
#define MPR_CMD_STDERR          2       /**< Stderr for the client side */
#define MPR_CMD_MAX_PIPE        3

/*
    Handler for command output and completion
 */
typedef void (*MprCmdProc)(struct MprCmd *cmd, int channel, void *data);

/*
    Flags for mprRunCmd
 */
#define MPR_CMD_NEW_SESSION     0x1     /**< mprRunCmd flag to create a new session on unix */
#define MPR_CMD_SHOW            0x2     /**< mprRunCmd flag to show the window of the created process on windows */
#define MPR_CMD_DETACH          0x4     /**< mprRunCmd flag to detach the child process and don't wait */
#define MPR_CMD_EXACT_ENV       0x8     /**< mprRunCmd flag to use the exact environment (no inherit from parent) */
#define MPR_CMD_IN              0x1000  /**< mprRunCmd flag to connect to stdin */
#define MPR_CMD_OUT             0x2000  /**< mprRunCmd flag to capture stdout */
#define MPR_CMD_ERR             0x4000  /**< mprRunCmd flag to capture stdout */

typedef struct MprCmdFile {
    char            *name;
    int             fd;
    int             clientFd;
#if ME_WIN_LIKE
    HANDLE          handle;
#endif
} MprCmdFile;

/**
    Command execution Service
    @description The MprCmd service enables execution of local commands. It uses three full-duplex pipes to communicate
        read, write and error data with the command.
    @stability Stable.
    @see mprCloseCmdFd mprCreateCmd mprDestroyCmd mprDisableCmdEvents mprDisconnectCmd mprEnableCmdEvents
        mprFinalizeCmd mprGetCmdBuf mprGetCmdExitStatus mprGetCmdFd mprIsCmdComplete mprIsCmdRunning
        mprReadCmd mprReapCmd mprRunCmd mprRunCmdV mprSetCmdCallback mprSetCmdDir mprSetCmdEnv mprSetCmdSearchPath
        mprStartCmd mprStopCmd mprWaitForCmd mprWriteCmd mprWriteCmdBlock
    @defgroup MprCmd MprCmd
    @stability Internal
 */
typedef struct MprCmd {
    cchar           *program;           /**< Program path name */
    int             pid;                /**< Process ID of the created process */
    int             originalPid;        /**< Persistent copy of the pid */
    int             status;             /**< Command exit status */
    int             flags;              /**< Control flags (userFlags not here) */
    int             eofCount;           /**< Count of end-of-files */
    int             requiredEof;        /**< Number of EOFs required for an exit */
    int             argc;               /**< Count of args in argv */
    int             timedout;           /**< Request has timedout */
    bool            complete: 1;        /**< All channels EOF and status gathered */
    bool            stopped: 1;         /**< Command stopped */
    cchar           **makeArgv;         /**< Allocated argv */
    cchar           **argv;             /**< List of args. Null terminated */
    char            *dir;               /**< Current working dir for the process */
    cchar           **defaultEnv;       /**< Environment to use if no env passed to mprStartCmd */
    char            *searchPath;        /**< Search path to use to locate the command */
    MprList         *env;               /**< List of environment variables. Null terminated. */
    MprCmdFile      files[MPR_CMD_MAX_PIPE]; /**< Stdin, stdout for the command */
    MprWaitHandler  *handlers[MPR_CMD_MAX_PIPE];
    MprDispatcher   *dispatcher;        /**< Dispatcher to use for wait events */
    MprCmdProc      callback;           /**< Handler for client output and completion */
    void            *callbackData;      /**< Managed callback data reference */
    MprForkCallback forkCallback;       /**< Forked client callback */
    MprSignal       *signal;            /**< Signal handler for SIGCHLD */
    void            *forkData;          /**< Managed fork callback data reference */
    MprBuf          *stdoutBuf;         /**< Standard output from the client */
    MprBuf          *stderrBuf;         /**< Standard error output from the client */
    void            *userData;          /**< User data storage */
    int             userFlags;          /**< User flags storage */
#if ME_WIN_LIKE
    char            *command;           /**< Windows command line */
    HANDLE          thread;             /**< Handle of the primary thread for the created process */
    HANDLE          process;            /**< Process handle for the created process */
#endif

#if VXWORKS
    /*
        Don't use MprCond so we can build single-threaded and still use MprCmd
     */
    SEM_ID          startCond;          /**< Synchronization semaphore for task start */
    SEM_ID          exitCond;           /**< Synchronization semaphore for task exit */
#endif
    MprMutex        *mutex;             /**< Multithread sync */
} MprCmd;

/**
    Return true if command events are enabled.
    @param cmd MprCmd object created via mprCreateCmd
    @param channel Channel number to close. Should be either MPR_CMD_STDIN, MPR_CMD_STDOUT or MPR_CMD_STDERR.
    @return true if I/O events are enabled for the given channel.
    @ingroup MprCmd
    @stability Internal
 */
PUBLIC bool mprAreCmdEventsEnabled(MprCmd *cmd, int channel);

/**
    Close the command channel
    @param cmd MprCmd object created via mprCreateCmd
    @param channel Channel number to close. Should be either MPR_CMD_STDIN, MPR_CMD_STDOUT or MPR_CMD_STDERR.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprCloseCmdFd(MprCmd *cmd, int channel);

/**
    Create a new Command object
    @returns A newly allocated MprCmd object.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC MprCmd *mprCreateCmd(MprDispatcher *dispatcher);

/**
    Destroy the command
    @param cmd MprCmd object created via mprCreateCmd
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprDestroyCmd(MprCmd *cmd);

/**
    Disable command I/O events. This disables events on a given channel.
    @param cmd MprCmd object created via mprCreateCmd
    @param channel Channel number to close. Should be either MPR_CMD_STDIN, MPR_CMD_STDOUT or MPR_CMD_STDERR.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprDisableCmdEvents(MprCmd *cmd, int channel);

/**
    Disconnect a command its underlying I/O channels. This is used to prevent further I/O wait events while
    still preserving the MprCmd object.
    @param cmd MprCmd object created via mprCreateCmd
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprDisconnectCmd(MprCmd *cmd);

/**
    Enable command I/O events. This enables events on a given channel.
    @param cmd MprCmd object created via mprCreateCmd
    @param channel Channel number to close. Should be either MPR_CMD_STDIN, MPR_CMD_STDOUT or MPR_CMD_STDERR.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprEnableCmdEvents(MprCmd *cmd, int channel);

/**
    Enable command I/O events for the command's STDOUT and STDERR channels
    @param cmd MprCmd object created via mprCreateCmd
    @param on Set to true to enable events. Set to false to disable.
    @return true if I/O events are enabled for the given channel.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprEnableCmdOutputEvents(MprCmd *cmd, bool on);

/**
    Finalize the writing of data to the command process
    @param cmd MprCmd object created via mprCreateCmd
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprFinalizeCmd(MprCmd *cmd);

/**
    Get the count of active commands.
    @description This is thread-safe with respect to MPR->state
    @return Count of running commands
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC ssize mprGetActiveCmdCount(void);

/**
    Get the underlying buffer for a channel
    @param cmd MprCmd object created via mprCreateCmd
    @param channel Channel number to close. Should be either MPR_CMD_STDIN, MPR_CMD_STDOUT or MPR_CMD_STDERR.
    @return A reference to the MprBuf buffer structure
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC MprBuf *mprGetCmdBuf(MprCmd *cmd, int channel);

/**
    Get the command exit status
    @param cmd MprCmd object created via mprCreateCmd
    @return status If the command has exited, a status between 0 and 255 is returned. Otherwise, a negative error
    code is returned.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC int mprGetCmdExitStatus(MprCmd *cmd);

/**
    Get the underlying file descriptor for an I/O channel
    @param cmd MprCmd object created via mprCreateCmd
    @param channel Channel number to close. Should be either MPR_CMD_STDIN, MPR_CMD_STDOUT or MPR_CMD_STDERR.
    @return The file descriptor
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC int mprGetCmdFd(MprCmd *cmd, int channel);

/**
    Test if a command is complete. A command is complete when the child has exited and all command output and error
    output has been received.
    @param cmd MprCmd object created via mprCreateCmd
 */
PUBLIC int mprIsCmdComplete(MprCmd *cmd);

/**
    Test if the command is still running.
    @param cmd MprCmd object created via mprCreateCmd
    @return True if the command is still running
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC bool mprIsCmdRunning(MprCmd *cmd);

#if ME_WIN_LIKE
/**
    Poll for I/O on the command pipes. This is only used on windows which cannot adequately detect EOF on a named pipe.
    @param cmd MprCmd object created via mprCreateCmd
    @param timeout Time in milliseconds to wait for the command to complete and exit.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprPollWinCmd(MprCmd *cmd, MprTicks timeout);

/**
   Start a timer calling mprPollWinCmd.
   @description This is useful for detached commands.
   @param cmd MprCmd object created via mprCreateCmd
   @ingroup MprCmd
   @stability Internal
 */
PUBLIC void mprStartWinPollTimer(MprCmd *cmd);
#endif

/**
    Make the I/O channels to send and receive data to and from the command.
    @param cmd MprCmd object created via mprCreateCmd
    @param channel Channel number to read from. Should be either MPR_CMD_STDIN, MPR_CMD_STDOUT or MPR_CMD_STDERR.
    @param buf Buffer to read into
    @param bufsize Size of buffer
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC ssize mprReadCmd(MprCmd *cmd, int channel, char *buf, ssize bufsize);

/**
    Reap the command. This waits for and collect the command exit status.
    @param cmd MprCmd object created via mprCreateCmd
    @param timeout Time in milliseconds to wait for the command to complete and exit.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC int mprReapCmd(MprCmd *cmd, MprTicks timeout);

/**
    Run a simple blocking command using a string command line.
    @param dispatcher MprDispatcher event queue to use for waiting. Set to NULL to use the default MPR dispatcher.
    @param command Command line to run
    @param input Command input. Data to write to the command which will be received on the comamnds stdin.
    @param output Reference to a string to receive the stdout from the command.
    @param error Reference to a string to receive the stderr from the command.
    @param timeout Time in milliseconds to wait for the command to complete and exit. Set to -1 to wait forever.
    @return Command exit status, or negative MPR error code.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC int mprRun(MprDispatcher *dispatcher, cchar *command, cchar *input, char **output, char **error, MprTicks timeout);

/**
    Run a command using a string command line. This starts the command via mprStartCmd() and waits for its completion.
    @param cmd MprCmd object created via mprCreateCmd
    @param command Command line to run
    @param envp Array of environment strings. Each environment string should be of the form: "KEY=VALUE". The array
        must be null terminated.
    @param in Command input. Data to write to the command which will be received on the comamnds stdin.
    @param out Reference to a string to receive the stdout from the command.
    @param err Reference to a string to receive the stderr from the command.
    @param timeout Time in milliseconds to wait for the command to complete and exit.
    @param flags Flags to modify execution. Valid flags are:
        MPR_CMD_NEW_SESSION     Create a new session on Unix
        MPR_CMD_SHOW            Show the commands window on Windows
        MPR_CMD_IN              Connect to stdin
        MPR_CMD_OUT             Capture stdout
        MPR_CMD_ERR             Capture stderr
        MPR_CMD_EXACT_ENV       Use the exact environment supplied. Don't inherit and blend with existing environment.
    @return Command exit status, or negative MPR error code.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC int mprRunCmd(MprCmd *cmd, cchar *command, cchar **envp, cchar *in, char **out, char **err, MprTicks timeout, int flags);

/**
    Run a command using an argv[] array of arguments. This invokes mprStartCmd() and waits for its completion.
    @param cmd MprCmd object created via mprCreateCmd
    @param argc Count of arguments in argv
    @param argv Command arguments array
    @param envp Array of environment strings. Each environment string should be of the form: "KEY=VALUE". The array
        must be null terminated.
    @param in Command input. Data to write to the command which will be received on the comamnds stdin.
    @param out Reference to a string to receive the stdout from the command.
    @param err Reference to a string to receive the stderr from the command.
    @param timeout Time in milliseconds to wait for the command to complete and exit.
    @param flags Flags to modify execution. Valid flags are:
        MPR_CMD_NEW_SESSION     Create a new session on Unix
        MPR_CMD_SHOW            Show the commands window on Windows
        MPR_CMD_IN              Connect to stdin
        MPR_CMD_OUT             Capture stdout
        MPR_CMD_ERR             Capture stderr
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC int mprRunCmdV(MprCmd *cmd, int argc, cchar **argv, cchar **envp, cchar *in, char **out, char **err,
    MprTicks timeout, int flags);

/**
    Define a callback to be invoked to receive response data from the command.
    @param cmd MprCmd object created via mprCreateCmd
    @param callback Function of the signature MprCmdProc which will be invoked for receive notification
        for data from the commands stdout and stderr channels. MprCmdProc has the signature:
        int callback(MprCmd *cmd, int channel, void *data) {}
    @param data User defined data to be passed to the callback.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprSetCmdCallback(MprCmd *cmd, MprCmdProc callback, void *data);

/**
    Set the default environment to use for commands.
    @description  This environment is used if one is not defined via #mprStartCmd
    @param cmd MprCmd object created via mprCreateCmd
    @param env Array of environment "KEY=VALUE" strings. Null terminated.
    @ingroup MprCmd
    @stability Stable
    @internal
 */
PUBLIC void mprSetCmdDefaultEnv(MprCmd *cmd, cchar **env);

/**
    Set the home directory for the command
    @param cmd MprCmd object created via mprCreateCmd
    @param dir String directory path name.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprSetCmdDir(MprCmd *cmd, cchar *dir);

/**
    Set the command environment
    @param cmd MprCmd object created via mprCreateCmd
    @param env Array of environment strings. Each environment string should be of the form: "KEY=VALUE". The array
        must be null terminated.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprSetCmdEnv(MprCmd *cmd, cchar **env);

/**
    Set the default command search path.
    @description The search path is used to locate the program to run for the command.
    @param cmd MprCmd object created via mprCreateCmd
    @param search Search string. This is in a format similar to the PATH environment variable.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC void mprSetCmdSearchPath(MprCmd *cmd, cchar *search);

/**
    Start the command. This starts the command but does not wait for its completion. Once started, mprWriteCmd
    can be used to write to the command and response data can be received via mprReadCmd.
    @param cmd MprCmd object created via mprCreateCmd
    @param argc Count of arguments in argv
    @param argv Command arguments array
    @param envp Array of environment strings. Each environment string should be of the form: "KEY=VALUE". The array
        must be null terminated.
    @param flags Flags to modify execution. Valid flags are:
        MPR_CMD_NEW_SESSION     Create a new session on Unix
        MPR_CMD_SHOW            Show the commands window on Windows
        MPR_CMD_IN              Connect to stdin
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC int mprStartCmd(MprCmd *cmd, int argc, cchar **argv, cchar **envp, int flags);

/**
    Stop the command. The command is immediately killed.
    @param cmd MprCmd object created via mprCreateCmd
    @param signal Signal to send to the command to kill if required
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC int mprStopCmd(MprCmd *cmd, int signal);

/**
    Wait for the command to complete.
    @param cmd MprCmd object created via mprCreateCmd
    @param timeout Time in milliseconds to wait for the command to complete and exit.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC int mprWaitForCmd(MprCmd *cmd, MprTicks timeout);

/**
    Write data to an I/O channel
    @description This is a non-blocking write and may return having written less than requested.
    @param cmd MprCmd object created via mprCreateCmd
    @param channel Channel number to read from. Should be either MPR_CMD_STDIN, MPR_CMD_STDOUT or MPR_CMD_STDERR.
    @param buf Buffer to read into
    @param bufsize Size of buffer
    @return Count of bytes written
    @ingroup MprCmd
    @stability Stable
 */
PUBLIC ssize mprWriteCmd(MprCmd *cmd, int channel, cchar *buf, ssize bufsize);

/**
    Write data to an I/O channel
    @description This is a blocking write.
    @param cmd MprCmd object created via mprCreateCmd
    @param channel Channel number to read from. Should be either MPR_CMD_STDIN, MPR_CMD_STDOUT or MPR_CMD_STDERR.
    @param buf Buffer to read into
    @param bufsize Size of buffer
    @return Count of bytes written
    @ingroup MprCmd
    @stability Stable
    @internal
 */
PUBLIC ssize mprWriteCmdBlock(MprCmd *cmd, int channel, cchar *buf, ssize bufsize);

/********************************** Cache *************************************/
/*
    General cache options
 */
#define MPR_CACHE_SHARED        0x1     /**< Use shared cache for mprCreateCache() */
#define MPR_CACHE_ADD           0x2     /**< mprWriteCache option to add key only if not already existing */
#define MPR_CACHE_SET           0x4     /**< mprWriteCache option to update key value, create if required */
#define MPR_CACHE_APPEND        0x8     /**< mprWriteCache option to set and append if already existing */
#define MPR_CACHE_PREPEND       0x10    /**< mprWriteCache option to set and prepend if already existing */

/*
    Notification events
 */
#define MPR_CACHE_NOTIFY_CREATE        1     /**< Item has been created */
#define MPR_CACHE_NOTIFY_REMOVE        2     /**< Item is about to be removed */
#define MPR_CACHE_NOTIFY_UPDATE        4     /**< Item has been updated */

/**
    Cache item expiry callback
    @param cache Cache object
    @param key Cached item key
    @param data Cached item data
    @param event Event of interest.
    @ingroup MprCache
    @stability Evolving
 */
typedef void (*MprCacheProc)(struct MprCache *cache, cchar *key, cchar *data, int event);

/**
    In-memory caching. The MprCache provides a fast, in-memory caching of cache items. Cache items are string key / value
    pairs. Cache items have a configurable lifespan and the Cache manager will automatically prune expired items.
    Items also have an associated version number that can be used when writing to do transactional writes.
    @defgroup MprCache MprCache
    @see mprCreateCache mprDestroyCache mprExpireCache mprIncCache mprReadCache mprRemoveCache mprSetCacheLimits
        mprWriteCache
    @stability Internal
 */
typedef struct MprCache {
    MprHash         *store;             /**< Key/value store */
    MprMutex        *mutex;             /**< Cache lock */
    MprEvent        *timer;             /**< Pruning timer */
    MprTicks        lifespan;           /**< Default lifespan (msec) */
    MprCacheProc    notify;             /* Notification callback for item expiry */
    int             resolution;         /**< Frequence for pruner */
    ssize           usedMem;            /**< Memory in use for keys and data */
    ssize           maxKeys;            /**< Max number of keys */
    ssize           maxMem;             /**< Max memory for session data */
    struct MprCache *shared;            /**< Shared common cache */
} MprCache;

/**
    Create a new cache object
    @param options Set of option flags. Use #MPR_CACHE_SHARED to select a global shared cache object.
    @return A cache instance object. On error, return null.
    @ingroup MprCache
    @stability Evolving
 */
PUBLIC MprCache *mprCreateCache(int options);

/**
    Initialize the cache service on startup. Should only be called by the MPR init on startup.
    @return Zero if successful.
    @stability Internal
 */
PUBLIC int mprCreateCacheService(void);

/**
    Destroy a new cache object
    @param cache The cache instance object returned from #mprCreateCache.
    @ingroup MprCache
    @stability Evolving
 */
PUBLIC void *mprDestroyCache(MprCache *cache);

/**
    Set the expiry date for a cache item
    @param cache The cache instance object returned from #mprCreateCache.
    @param key Cache item key
    @param expires Time when the cache item will expire. If expires is zero, the item is immediately removed from the cache.
    @return Zero if the expiry is successfully updated. Return MPR_ERR_CANT_FIND if the cache item is not present in the
        cache.
    @ingroup MprCache
    @stability Evolving
 */
PUBLIC int mprExpireCacheItem(MprCache *cache, cchar *key, MprTicks expires);

/**
    Get the Cache statistics
    @param cache The cache instance object returned from #mprCreateCache.
    @param numKeys Number of keys currently stored
    @param mem Memory in use to store keys
    @ingroup MprCache
    @stability Evolving
    @internal
 */
PUBLIC void mprGetCacheStats(MprCache *cache, int *numKeys, ssize *mem);

/**
    Increment a numeric cache item
    @param cache The cache instance object returned from #mprCreateCache.
    @param key Cache item key
    @param amount Numeric amount to increment the cache item. This may be a negative number to decrement the item.
    @return The new value for the cache item after incrementing.
    @ingroup MprCache
    @stability Evolving
 */
PUBLIC int64 mprIncCache(MprCache *cache, cchar *key, int64 amount);

/**
    Lookup an item in the cache.
    @description Same as mprReadCache but will not update the last accessed time.
    @param cache The cache instance object returned from #mprCreateCache.
    @param key Cache item key
    @param modified Optional MprTime value reference to receive the last modified time of the cache item. Set to null
        if not required.
    @param version Optional int64 value reference to receive the version number of the cache item. Set to null
        if not required. Cache items have a version number that is incremented every time the item is updated.
    @return The cache item value
    @ingroup MprCache
    @stability Evolving
  */
PUBLIC char *mprLookupCache(MprCache *cache, cchar *key, MprTime *modified, int64 *version);

/**
    Prune the cache
    @description Prune the cache and discard all cached items
    @param cache The cache instance object returned from #mprCreateCache.
    @ingroup MprCache
    @stability Evolving
 */
PUBLIC void mprPruneCache(MprCache *cache);

/**
    Read an item from the cache.
    @param cache The cache instance object returned from #mprCreateCache.
    @param key Cache item key
    @param modified Optional MprTime value reference to receive the last modified time of the cache item. Set to null
        if not required.
    @param version Optional int64 value reference to receive the version number of the cache item. Set to null
        if not required. Cache items have a version number that is incremented every time the item is updated.
    @return The cache item value
    @ingroup MprCache
    @stability Evolving
  */
PUBLIC char *mprReadCache(MprCache *cache, cchar *key, MprTime *modified, int64 *version);

/**
    Remove items from the cache
    @param cache The cache instance object returned from #mprCreateCache.
    @param key Cache item key. If set to null, then remove all keys from the cache.
    @return True if the cache item was removed.
    @ingroup MprCache
    @stability Evolving
  */
PUBLIC bool mprRemoveCache(MprCache *cache, cchar *key);

/**
    Set a notification callback to be invoked for events of interest on cached items.
    WARNING: the callback may happen on any thread. Use careful locking to synchronize access to data. Take care
        not to block the thread issuing the callback.
    @param cache The cache instance object returned from #mprCreateCache.
    @param notify MprCacheProc notification callback. Invoked for events of interest on cache items.
        The event is set to MPR_CACHE_NOTIFY_REMOVE when items are removed from the cache.  Invoked as:

        (*MprCacheProc)(MprCache *cache, cchar *key, cchar *data, int event);
    @ingroup MprCache
    @stability Evolving
  */
PUBLIC void mprSetCacheNotify(MprCache *cache, MprCacheProc notify);

/**
    Set the cache resource limits
    @param cache The cache instance object returned from #mprCreateCache.
    @param keys Set the maximum number of keys the cache can store
    @param lifespan Set the default lifespan for cache items in milliseconds
    @param memory Memory limit in bytes for all cache keys and items.
    @param resolution Set the cache item pruner resolution. This defines how frequently the cache manager will check
        items for expiration.
    @ingroup MprCache
    @stability Evolving
  */
PUBLIC void mprSetCacheLimits(MprCache *cache, int64 keys, MprTicks lifespan, int64 memory, int resolution);

/**
    Set a linked managed memory reference for a cached item.
    @param cache The cache instance object returned from #mprCreateCache.
    @param key Cache item key to write
    @param link Managed memory reference. May be NULL.
    @ingroup MprCache
    @stability Evolving
 */
PUBLIC int mprSetCacheLink(MprCache *cache, cchar *key, void *link);

/**
    Write a cache item
    @param cache The cache instance object returned from #mprCreateCache.
    @param key Cache item key to write
    @param value Value to set for the cache item. This must be allocated memory.
    @param modified Value to set for the cache last modified time. If set to zero, the current time is obtained via
        #mprGetTime.
    @param lifespan Lifespan of the item in milliseconds. The item will be removed from the cache by the Cache manager
        when the lifetime expires unless it is rewritten to extend the lifespan.
    @param version Expected version number of the item. This is used to do transactional writes to the cache item.
        First the version number is retrieved via #mprReadCache and that version number is supplied to mprWriteCache when
        the item is updated. If another caller updates the item in between the read/write, the version number will not
        match when the item is subsequently written and this call will fail with the #MPR_ERR_BAD_STATE return code. Set to
        zero if version checking is not required.
    @param options Options to control how the item value is updated. Use #MPR_CACHE_SET to update the cache item and
        create if it does not exist. Use #MPR_CACHE_ADD to add the item only if it does not already exits. Use
        #MPR_CACHE_APPEND to append the parameter value to any existing cache item value. Use #MPR_CACHE_PREPEND to
        prepend the value.
    @return If writing the cache item was successful this call returns the number of bytes written. Otherwise a negative
        MPR error code is returned. #MPR_ERR_BAD_STATE will be returned if an invalid version number is supplied.
        #MPR_ERR_ALREADY_EXISTS will be returned if #MPR_CACHE_ADD is specified and the cache item already exists.
    @ingroup MprCache
    @stability Evolving
 */
PUBLIC ssize mprWriteCache(MprCache *cache, cchar *key, cchar *value, MprTime modified, MprTicks lifespan,
        int64 version, int options);

/******************************** Mime Types **********************************/
/**
    Mime Type hash table entry (the URL extension is the key)
    @stability Stable
    @defgroup MprMime MprMime
    @see MprMime mprAddMime mprCreateMimeTypes mprGetMimeProgram mprLookupMime mprSetMimeProgram
    @stability Internal
 */
typedef struct MprMime {
    char    *type;                          /**< Mime type string */
    char    *program;                       /**< Mime type string */
} MprMime;

/**
    Add a mime type to the mime type table
    @param table type hash table returned by #mprCreateMimeTypes
    @param ext Filename extension to use as a key for the given mime type
    @param mimeType Mime type string to associate with the ext key
    @return Mime type entry object. This is owned by the mime type table.
    @ingroup MprMime
    @stability Stable
 */
PUBLIC MprMime *mprAddMime(MprHash *table, cchar *ext, cchar *mimeType);

/**
    Create the mime types
    @param path Filename of a mime types definition file
    @return Hash table of mime types keyed by file extension
    @ingroup MprMime
    @stability Stable
 */
PUBLIC MprHash *mprCreateMimeTypes(cchar *path);

/**
    Get the mime type program for a given mimeType
    @param table type hash table returned by #mprCreateMimeTypes
    @param mimeType Mime type to update
    @return The program name associated with this mime type
    @ingroup MprMime
    @stability Stable
 */
PUBLIC cchar *mprGetMimeProgram(MprHash *table, cchar *mimeType);

/**
    Get the mime type for an extension.
    This call will return the mime type from a limited internal set of mime types for the given path or extension.
    @param table Hash table of mime types to examine
    @param ext Path or extension to examine
    @returns Mime type string. Returns null if mime type is not known.
    @ingroup MprMime
    @stability Stable
 */
PUBLIC cchar *mprLookupMime(MprHash *table, cchar *ext);

/**
    Set the mime type program
    @param table type hash table returned by #mprCreateMimeTypes
    @param mimeType Mime type to update
    @param program Program name to associate with this mime type
    @return Zero if the update is successful. Otherwise return MPR_ERR_CANT_FIND if the mime type is not present in
        the mime type table.
    @ingroup MprMime
    @stability Stable
 */
PUBLIC int mprSetMimeProgram(MprHash *table, cchar *mimeType, cchar *program);

/************************************ MPR *************************************/
/*
    Mpr state
 */
#define MPR_CREATED         1           /**< Applicationa and MPR services started */
#define MPR_STARTED         2           /**< Applicationa and MPR services started */
#define MPR_STOPPING        3           /**< App has been instructed to shutdown. Services should not accept new requests */
#define MPR_STOPPED         4           /**< App is idle and now stopped. All requests should abort. */
#define MPR_DESTROYING      5           /**< Destroying core MPR services and releasing memory */
#define MPR_DESTROYED       6           /**< Application and MPR object destroyed  */

/*
    MPR flags
 */
#define MPR_LOG_ANEW        0x1         /**< Start anew on restart after backup */
#define MPR_LOG_CONFIG      0x2         /**< Show the configuration at the start of the log */
#define MPR_LOG_CMDLINE     0x4         /**< Command line log switch uses */
#define MPR_LOG_DETAILED    0x8         /**< Use detailed log formatting with timestamps and tags */
#define MPR_LOG_TAGGED      0x10        /**< Use tagged message formatting */
#define MPR_LOG_HEXDUMP     0x10        /**< Emit hexdump */

#define MPR_NOT_ALL         0x20        /**< Don't invoke all destructors when terminating */

typedef bool (*MprIdleCallback)(bool traceRequests);

/**
    Service shutdown notifier
    @description Services may create shutdown notifiers, called terminators that are informed when the application
        commences a shutdown. The terminator may be invoked several times and the service should take appropriate
        action based on given the MPR state.
        \n\n
        If the state parameter is set to MPR_STOPPING, the service should not accept any new requests, but otherwise not take
            any destructive actions. Note this state is required to be reversible if the shutdown is cancelled.
        \n\n
        If the state is MPR_STOPPED, the service should cancel all running requests, close files and connections and release
        all resources. This state is not reversible.
        \n\n
        This exitStrategy parameter is a flags word that defines the shutdown strategy. See #mprShutdown for details.
    @param state Current MPR state. Set to #MPR_STARTED, #MPR_STOPPING, #MPR_STOPPED and #MPR_DESTROYED.
    @param exitStrategy Flags word including the flags: MPR_EXIT_ABORT, MPR_EXIT_RESTART and MPR_EXIT_SAFE.
    @param status The desired application exit status
    @ingroup Mpr
    @stability Stable
  */
typedef void (*MprTerminator)(int state, int exitStrategy, int status);

/**
    Primary MPR application control structure
    @description The Mpr structure stores critical application state information.
    @see mprAddTerminator mprBreakpoint mprCreate mprCreateOsService mprDecode64 mprDestroy mprEmptyString mprEncode64
    mprEscapeCmd mprEscapseHtml mprGetApp mprGetAppDir mprGetAppName mprGetAppPath mprGetAppTitle mprGetAppVersion
    mprGetCmdlineLogging mprGetDebugMode mprGetDomainName mprGetEndian mprGetError mprGetHostName
    mprGetHwnd mprGetInst mprGetIpAddr mprGetKeyValue mprGetLogLevel mprGetMD5 mprGetMD5WithPrefix mprGetOsError
    mprGetRandomBytes mprGetServerName mprIsDestroyed mprIsIdle mprIsStopping mprIsDestroying mprMakeArgv
    mprRandom mprReadRegistry mprRemoveKeyValue mprRestart mprServicesAreIdle mprSetAppName
    mprSetDebugMode mprSetDomainName mprSetHostName mprSetHwnd mprSetIdleCallback mprSetInst
    mprSetIpAddr mprSetLogLevel mprSetServerName mprSetSocketMessage mprShouldAbortRequests mprShouldDenyNewRequests
    mprSignalExit mprSleep mprStart mprStartEventsThread mprStartOsService mprStopOsService mprShutdown mprUriDecode
    mprUriDecodeBuf mprUriEncode mprWriteRegistry
    @defgroup Mpr Mpr
    @stability Internal.
 */
typedef struct Mpr {
    MprHeap         *heap;                  /**< Memory heap control */
    MprLogHandler   logHandler;             /**< Current log handler callback */
    MprFile         *logFile;               /**< Log file */
    MprHash         *mimeTypes;             /**< Table of mime types */
    MprHash         *timeTokens;            /**< Date/Time parsing tokens */
    MprHash         *keys;                  /**< Simple key/value store */
    MprFile         *stdError;              /**< Standard error file */
    MprFile         *stdInput;              /**< Standard input file */
    MprFile         *stdOutput;             /**< Standard output file */
    MprTime         start;                  /**< When the MPR started */
    MprTicks        exitTimeout;            /**< Request timeout when exiting */
    ssize           logSize;                /**< Maximum log size */
    cchar           *appPath;               /**< Path name of application executable */
    cchar           *appDir;                /**< Path of directory containing app executable */
    cchar           **argv;                 /**< Application command line args (not alloced) */
    char            **argBuf;               /**< Space for allocated argv */
    cchar           *logPath;               /**< Log path name */
    char            *pathEnv;               /**< Cached PATH env var. Used by MprCmd */
    char            *name;                  /**< Product name */
    char            *title;                 /**< Product title */
    char            *version;               /**< Product version */
    char            *domainName;            /**< Domain portion */
    char            *hostName;              /**< Host name (fully qualified name) */
    char            *ip;                    /**< Public IP Address */
    char            *serverName;            /**< Server name portion (no domain) */
    int             argc;                   /**< Count of command line args */
    int             eventing;               /**< Servicing events thread is active */
    int             exitStrategy;           /**< How to exit the app */
    int             flags;                  /**< Misc flags */
    int             hasError;               /**< Mpr has an initialization error */
    int             logLevel;               /**< Log trace level */
    int             logBackup;              /**< Number of log files preserved when backing up */
    int             verifySsl;              /**< Default verification of SSL certificates */
    bool            debugMode;              /**< Run in debug mode (no timers) */

    /*
        Service pointers
     */
    struct MprCmdService    *cmdService;    /**< Command service object */
    struct MprEventService  *eventService;  /**< Event service object */
    struct MprModuleService *moduleService; /**< Module service object */
    struct MprOsService     *osService;     /**< O/S service object */
    struct MprSignalService *signalService; /**< Signal service object */
    struct MprSocketService *socketService; /**< Socket service object */
    struct MprThreadService *threadService; /**< Thread service object */
    struct MprWorkerService *workerService; /**< Worker service object */
    struct MprWaitService   *waitService;   /**< IO Waiting service object */

    struct MprDispatcher    *dispatcher;    /**< Primary dispatcher */
    struct MprDispatcher    *nonBlock;      /**< Nonblocking dispatcher */

    /*
        These are here to optimize access to these singleton service objects
     */
    void            *appwebService;         /**< Appweb service object */
    void            *ediService;            /**< EDI object */
    void            *ejsService;            /**< Ejscript service */
    void            *espService;            /**< ESP service object */
    void            *httpService;           /**< Http service object */

    MprList         *fileSystems;           /**< File system objects */
    MprTicks        shutdownStarted;        /**< When the shutdown started */
    MprList         *terminators;           /**< Termination callbacks */
    MprIdleCallback idleCallback;           /**< Invoked to determine if the process is idle */
    MprOsThread     mainOsThread;           /**< Main OS thread ID */
    MprMutex        *mutex;                 /**< Thread synchronization used for global lock */
    MprSpin         *spin;                  /**< Quick thread synchronization */
    MprCond         *cond;                  /**< Sync after starting events thread */
    MprCond         *stopCond;              /**< Sync for stopping */
    char            *emptyString;           /**< "" string */
    char            *oneString;             /**< "1" string */
#if ME_WIN_LIKE
    HINSTANCE       appInstance;            /**< Application instance (windows) */
#endif
    MprFileSystem   *romfs;                /**< Rom file system object */
} Mpr;

PUBLIC void mprNop(void *ptr);

#if DOXYGEN || ME_WIN_LIKE
    /**
        Return the MPR control instance.
        @description Return the MPR singleton control object.
        @return Returns the MPR control object.
        @ingroup Mpr
        @stability Stable.
     */
    PUBLIC Mpr *mprGetMpr(void);
    #define MPR mprGetMpr()
#else
    #define mprGetMpr() MPR
    PUBLIC_DATA Mpr *MPR;
#endif

#define MPR_DISABLE_GC          0x1         /**< Disable GC */
#define MPR_USER_EVENTS_THREAD  0x2         /**< User will explicitly manage own mprServiceEvents calls */
#define MPR_NO_WINDOW           0x4         /**< Don't create a windows Window */
#define MPR_DELAY_GC_THREAD     0x8         /**< Delay starting the GC thread */
#define MPR_DAEMON              0x10        /**< Make the process a daemon */

/**
    Add a service terminator
    @description Services may create shutdown notifiers called terminators that are informed when the application commences a shutdown.
        The terminator may be invoked several times and the service should take appropriate action based on the MPR state.
        \n\n
        If the state parameter is set to MPR_STOPPING, the service should not accept any new requests, but otherwise not take
            any destructive actions. Note this state is required to be reversible if the shutdown is cancelled.
        \n\n
        If the state is MPR_STOPPED, the service should cancel all running requests, close files and connections and release
        all resources. This state is not reversible.
        \n\n
        This exitStrategy parameter is a flags word that defines the shutdown exit strategy. See #mprShutdown for details.
        \n\n
        Services may also call #mprShouldDenyNewRequests to test if the MPR state is MPR_STOPPING and #mprShouldAbortRequests
        if the state is MPR_STOPPED.
    @param terminator MprTerminator callback function
    @ingroup Mpr
    @stability Stable.
  */
PUBLIC void mprAddTerminator(MprTerminator terminator);

/**
    Initialize the application by creating an instance of the MPR.
    @description Initializes the MPR and creates an Mpr control object. The Mpr Object manages all MPR facilities
        and services. This must be called before using any MPR API. When processing is complete, you should call
        #mprDestroy before exiting the application.
    @param argc Count of command line args
    @param argv Command line arguments for the application. Arguments may be passed into the Mpr for retrieval
        by the unit test framework.
    @param flags Set MPR_USER_EVENTS_THREAD if you will manage calling #mprServiceEvents manually if required.
    There are three styles of MPR applications with respect to servicing events:
    \n\n
    1) Applications that don't require servicing events for I/O, commands or timers
    \n\n
    2) Applications that call #mprServiceEvents directly from their main program
    \n\n
    3) Applications that have a dedicated service events thread
    \n\n
    Applications that do not perform I/O, run commands or create events may not need a service events thread.
    While creating one will do no harm, performance may be enhanced for these applications by specifying MPR_USER_EVENTS_THREAD.
    \n\n
    Applications that have not forground processing requirements may invoke #mprServiceEvents from their main program instead
    of creating a service events thread. This saves one thread.
    \n\n
    The default is to create a service events thread so the full scope of MPR services are supported.
    @return Returns a pointer to the Mpr object.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC Mpr *mprCreate(int argc, char **argv, int flags);

/**
    Convert the process into a daemon on unix systems
    @description This converts the current process into a detached child without a parent.
    @returns Zero if successful. Otherwise a negative MPR error code.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC int mprDaemon(void);

/**
    Destroy the MPR and all services using the MPR.
    @description This call terminates the MPR and all services.
    \n\n
    An application initializes the MPR by calling #mprCreate. This creates the Mpr object, the memory allocator, garbage collector
    and other services. An application exits by invoking #mprDestroy or by calling #mprShutdown then #mprDestroy.
    \n\n
    There are two styles of MPR applications with respect to shutdown:
    \n\n
    1) Applications that have a dedicated service events thread.
    \n\n
    2) Applications that call #mprServiceEvents directly from their main program.
    \n\n
    Applications that have a service events thread can call mprDestroy directly from their main program when ready to exit.
    Applications that call mprServiceEvents from their main program will typically have some other MPR thread call
    #mprShutdown to initiate a shutdown sequence. This will stop accepting new requests or connections and when the
    application is idle, the #mprServiceEvents routine will return and then the main program can call then call mprDestroy.
    \n\n
    Once the shutdown conditions are satisfied, a thread executing #mprServiceEvents will return from that API and then
    the application should call #mprDestroy and exit().
    \n\n
    If an application needs to tailor how it exits with respect to current requests, use #mprShutdown first to specify a
    shutdown strategy.
    @return True if the MPR can be destroyed. Returns false if the exit strategy MPR_EXIT_SAFE has been defined via
        #mprShutdown and current requests have not completed within the exit timeout
        period defined by #mprSetExitTimeout. In this case, the shutdown is cancelled and normal operations continue.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC bool mprDestroy(void);

/**
    Reference to a permanent preallocated empty string.
    @return An empty string
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC char *mprEmptyString(void);

/**
    Get the application directory
    @description Get the directory containing the application executable.
    @returns A string containing the application directory.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC cchar *mprGetAppDir(void);

/**
    Get the application name defined via mprSetAppName
    @returns the one-word lower case application name defined via mprSetAppName
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC cchar *mprGetAppName(void);

/**
    Get the application executable path
    @returns A string containing the application executable path.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC cchar *mprGetAppPath(void);

/**
    Get the application title string
    @returns A string containing the application title string.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC cchar *mprGetAppTitle(void);

/**
    Get the application version string
    @returns A string containing the application version string.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC cchar *mprGetAppVersion(void);

/**
    Get if command line logging is being used.
    @description Logging may be initiated by invoking an MPR based program with a "--log" switch. This API assists
        programs to tell the MPR that command line logging has been used.
    @return True if command line logging is in use.
    @ingroup Mpr
    @stability Stable.
*/
PUBLIC bool mprGetCmdlineLogging(void);

/**
    Get the debug mode.
    @description Returns whether the debug mode is enabled. Some modules
        observe debug mode and disable timeouts and timers so that single-step
        debugging can be used.
    @return Returns true if debug mode is enabled, otherwise returns false.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC bool mprGetDebugMode(void);

/**
    Get the application domain name string
    @returns A string containing the application domain name string.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC cchar *mprGetDomainName(void);

/**
    Return the endian byte ordering for the application
    @return MPR_LITTLE_ENDIAN or MPR_BIG_ENDIAN.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprGetEndian(void);

/**
    Return the error code for the most recent system or library operation.
    @description Returns an error code from the most recent system call.
        This will be mapped to be either a POSIX error code or an MPR error code.
    @return The mapped error code.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprGetError(void);

/**
    Get the exit status
    @description Get the exit status set via #mprShutdown
    May be called after #mprDestroy.
    @return The proposed application exit status
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprGetExitStatus(void);

/**
    Get the application host name string
    @returns A string containing the application host name string.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC cchar *mprGetHostName(void);

/**
    Get the application IP address string
    @returns A string containing the application IP address string.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC cchar *mprGetIpAddr(void);

/**
    Get a key value
    @param key String key value
    @ingroup Mpr
    @stability Stable
 */
PUBLIC void *mprGetKey(cchar *key);

/**
    Get the current logging level
    @return The current log level.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprGetLogLevel(void);

/**
    Get some random data
    @param buf Reference to a buffer to hold the random data
    @param size Size of the buffer
    @param block Set to true if it is acceptable to block while accumulating entropy sufficient to provide good
        random data. Setting to false will cause this API to not block and may return random data of a lower quality.
    @ingroup Mpr
    @stability Stable.
  */
PUBLIC int mprGetRandomBytes(char *buf, ssize size, bool block);

/**
    Get some random data in ascii
    @param size Size of the random data string
    @ingroup Mpr
    @stability Stable.
  */
PUBLIC char *mprGetRandomString(ssize size);

/**
    Return the O/S error code.
    @description Returns an O/S error code from the most recent system call.
        This returns errno on Unix systems or GetLastError() on Windows..
    @return The O/S error code.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprGetOsError(void);

/**
    Get the application server name string
    @returns A string containing the application server name string.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC cchar *mprGetServerName(void);

/**
    Get the MPR execution state
    @returns MPR_CREATED, MPR_STARTED, MPR_STOPPING, MPR_STOPPED, MPR_DESTROYING, or MPR_DESTROYED.
    @ingroup Mpr
    @stability Stable
  */
PUBLIC int mprGetState(void);

/**
    Determine if the MPR has finished.
    @description This is true if the MPR services have been shutdown completely. This is typically
        used to determine if the App has been shutdown.
    @returns True if the App has been instructed to exit and all the MPR services have been destroyed.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC bool mprIsDestroyed(void);

/**
    Determine if the App is idle.
    @description This call returns true if the App is not currently servicing any requests. By default this returns true
    if the MPR dispatcher, worker thread and command subsytems are idle. Callers can replace or augment the standard
    idle testing by definining a new idle callback via mprSetIdleCallback.
    \n\n
    Note: this routine tests for worker threads but ignores other threads created via #mprCreateThread.
    @param traceRequests If true, emit trace regarding running requests.
    @return True if the App are idle.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC bool mprIsIdle(bool traceRequests);

/**
    Test if the application is stopping
    If mprIsStopping is true, the application has commenced a shutdown. No new requests should be accepted and current request
    should complete if possible. Use #mprIsDestroyed to test if the application has completed its shutdown.
    @return True if the application is in the process of exiting
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC bool mprIsStopping(void);

/**
    Test if the application is stopped
    If this routine returns true, the application shutdown has passed the point of no return.
        No new requests should be accepted and current requests should be aborted.
        Use #mprIsStopping to test if shutdown has been initiated but current requests may continue.
        Use #mprIsDestroyed to test if the application has completed its shutdown.
    @return True if the application is in the process of exiting
    @ingroup Mpr
    @stability Stable
 */
PUBLIC bool mprIsStopped(void);

/**
    Test if the application is terminating and core services are being destroyed
    All request should immediately terminate.
    @return True if the application is in the process of exiting and core services should also exit.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC bool mprIsDestroying(void);

#define MPR_ARGV_ARGS_ONLY    0x1     /**< Command is missing program name */

/**
    Make a argv style array of command arguments
    @description The given command is parsed and broken into separate arguments and returned in a null-terminated, argv
        array. Arguments in the command may be quoted with single or double quotes to group words into one argument.
        Use back-quote "\\" to escape quotes.
        This routine allocates memory and must not be called before #mprCreate. Consider #mprParseArgs if you need to convert
        a command line before calling #mprCreate.
    @param command Command string to parse.
    @param argv Output parameter containing the parsed arguments.
    @param flags Set to MPR_ARGV_ARGS_ONLY if the command string does not contain a program name. In this case, argv[0]
        will be set to "".
    @return The count of arguments in argv
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprMakeArgv(cchar *command, cchar ***argv, int flags);

/**
    Nap for a while
    @description This routine blocks and does not yield for GC. Only use it for very short naps.
    @param msec Number of milliseconds to sleep
    @ingroup Mpr
    @stability Stable.
*/
PUBLIC void mprNap(MprTicks msec);

/**
    Make a argv style array of command arguments
    @description The given command is parsed and broken into separate arguments and returned in a null-terminated, argv
        array. Arguments in the command may be quoted with single or double quotes to group words into one argument.
        Use back-quote "\\" to escape quotes. This routine modifies supplied command parameter and does not allocate
        any memory and may be used before mprCreate is invoked.
    @param command Command string to parse.
    @param argv Array for the arguments.
    @param maxArgs Size of the argv array.
    @return The count of arguments in argv
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprParseArgs(char *command, char **argv, int maxArgs);

/**
    Restart the application
    @description This call immediately restarts the application. The standard input, output and error I/O channels are
    preserved. All other open file descriptors are closed.
    \n\n
    If the application is started via a monitoring launch daemon such as launchd or appman, the application should not use
    this API, but rather defer to the launch daemon to restart the application. In that case, the application should simply
    do a shutdown via #mprShutdown and/or #mprDestroy.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprRestart(void);

/**
    Determine if the MPR services.
    @description This is the default routine invoked by mprIsIdle().
    @param traceRequests If true, emit trace regarding running requests.
    @return True if the MPR services are idle.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC bool mprServicesAreIdle(bool traceRequests);

/**
    Set the application name, title and version
    @param name One word, lower case name for the app.
    @param title Pascal case multi-word descriptive name.
    @param version Version of the app. Major-Minor-Patch. E.g. 1.2.3.
    @returns Zero if successful. Otherwise a negative MPR error code.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprSetAppName(cchar *name, cchar *title, cchar *version);

/**
    Set the application executable path
    @param path A string containing the application executable path.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetAppPath(cchar *path);

/**
    Set if command line logging was requested.
    @description Logging may be initiated by invoking an MPR based program with a "--log" switch. This API assists
        programs to tell the MPR that command line logging has been used.
    @param on Set to true to indicate command line logging is being used.
    @return True if command line logging was enabled before this call.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC bool mprSetCmdlineLogging(bool on);

/**
    Turn on debug mode.
    @description Debug mode disables timeouts and timers. This makes debugging
        much easier.
    @param on Set to true to enable debugging mode.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetDebugMode(bool on);

/**
    Set the proposed exit status
    @description Set the exit status that can be retrieved via #mprGetExitStatus.
    @param status Proposed exit status value.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC void mprSetExitStatus(int status);

/**
    Set the current logging verbosity level.
    @description This call defines the maximum level of messages that will be
        logged. Calls to mprLog specify a message level. If the message level
        is greater than the defined logging level, the message is ignored.
    @param level New logging level. Must be 0-5 inclusive.
    @return Returns the previous logging level.
    @ingroup MprLog
    @stability Stable.
 */
PUBLIC void mprSetLogLevel(int level);

/**
    Set the application domain name string
    @param s New value to use for the application domain name.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetDomainName(cchar *s);

/**
    Set an environment variable value
    @param key Variable name
    @param value Variable value
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetEnv(cchar *key, cchar *value);

/**
    Set the error code.
    @description Set errno or equivalent.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetError(int error);

/**
    Set the exit timeout for a shutdown.
    @description A shutdown waits for existing requests to complete before exiting. After this timeout has expired,
        the application will either invoke exit() or cancel the shutdown depending on whether MPR_EXIT_SAFE is defined in
        the exit strategy via #mprShutdown.
        The default exit timeout is zero.
    @param timeout Time in milliseconds to wait for current requests to complete and the application to become idle.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetExitTimeout(MprTicks timeout);

/**
    Set the maximum number of open file/socket descriptors
    @param limit Limit to enforce
    @ingroup Mpr
    @stability Stable
 */
PUBLIC void mprSetFilesLimit(int limit);

/**
    Set the application host name string. This is internal to the application and does not affect the O/S host name.
    @param s New host name to use within the application
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetHostName(cchar *s);

/**
    Define a new idle callback to be invoked by mprIsIdle().
    @param idleCallback Callback function to invoke to test if the application is idle.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC MprIdleCallback mprSetIdleCallback(MprIdleCallback idleCallback);

/**
    Sete the application IP address string
    @param ip IP address string to store for the application
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetIpAddr(cchar *ip);

/**
    Store a key/value pair
    @param key String key value
    @param value Manage object reference
    @ingroup Mpr
    @stability Stable
 */
PUBLIC void mprSetKey(cchar *key, void *value);

/**
    Set the O/S error code.
    @description Set errno or equivalent.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetOsError(int error);

/**
    Set the application server name string
    @param s New application server name to use within the application.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetServerName(cchar *s);

/**
    Test if requests should be aborted.
    @description This routine indicates that current requests should be terminated due to an application shutdown.
    This will be true then the MPR->state >= MPR_EXIT_STOPPED.
    See also #mprShouldDenyNewRequests.
    @return True if new requests should be denied.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC bool mprShouldAbortRequests(void);

/**
    Test if new requests should be denied.
    @description This routine indicates if an application shutdown has been initiated and services should not
    accept new requests or connections.
    This will be true then the MPR->state >= MPR_EXIT_STOPPING.
    See also #mprShouldAbortRequests.
    @return True if new requests should be denied.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC bool mprShouldDenyNewRequests(void);

/**
    Sleep for a while
    @description This routine blocks for the given time and yields for GC during that time. Ensure all memory
        is held before the sleep.
    @param msec Number of milliseconds to sleep
    @ingroup Mpr
    @stability Stable.
*/
PUBLIC void mprSleep(MprTicks msec);

/**
    Start the Mpr services
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprStart(void);

/**
    Start an thread dedicated to servicing events. This will create a new thread and invoke mprServiceEvents.
    @return Zero if successful.
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC int mprStartEventsThread(void);

/*
    Shutdown flags
 */
#define MPR_EXIT_NORMAL     0x0         /**< Normal (graceful) exit */
#define MPR_EXIT_ABORT      0x1         /**< Abort everything and call exit() */
#define MPR_EXIT_SAFE       0x2         /**< Graceful shutdown only if all requests complete */
#define MPR_EXIT_RESTART    0x4         /**< Restart after exiting */

#define MPR_EXIT_TIMEOUT    -1          /**< Use timeout specified via #mprSetExitTimeout */

/**
    Initiate shutdown of the MPR and application.
    @description Commence shutdown of the application according to the shutdown policy defined by the "exitStrategy" parameter.
    An application may call this routine from any thread to request the application exit. Depending on the exitStrategy, this
    may be an abortive or graceful exit. A desired application exit status code can defined to indicate the cause of the shutdown.
    \n\n
    Once called, this routine will set the MPR execution state to MPR_EXIT_STOPPING. Services should detect this by calling
    #mprShouldDenyNewRequests before accepting new connections or requests, but otherwise, services should not take any destructive
    actions until the MPR state is advanced to MPR_EXIT_STOPPED by #mprDestroy. This state can be detected by calling
    #mprShouldAbortRequests. Users can invoke #mprCancelShutdown to resume normal operations provided #mprDestroy has not
    proceeded past the point of no return when destructive termination actions are commenced.
    \n\n
    Applications that have a user events thread and call #mprServiceEvents from their main program, will typically invoke
    mprShutdown from some other MPR thread to initiate the shutdown. When running requests have completed, or when the
    shutdown timeout expires (MPR->exitTimeout), the call to #mprServiceEvents in the main program will return and
    the application can then call #mprDestroy to complete the shutdown.
    \n\n
    Note: This routine starts the shutdown process but does not perform any destructive actions.
    @param exitStrategy Shutdown policy.
    If the MPR_EXIT_ABORT flag is specified, the application will immediately call exit() and will terminate without
    waiting for current requests to complete. This is not recommended for normal operation as data may be lost.
    \n\n
    If MPR_EXIT_SAFE is defined, the shutdown will be cancelled if all requests do not complete before the exit timeout
    defined via #mprSetExitTimeout expires.
    \n\n
    Define the MPR_EXIT_RESTART flag for the application to automatically restart after exiting. Do not use this option if
    the application is using a watchdog/angel process to automatically restart the application (such as appman by appweb).
    @param status Proposed exit status to use when the application exits. See #mprGetExitStatus.
    @param timeout Exit timeout in milliseconds to wait for current requests to complete. If set to -1, for the default exit timeout.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC void mprShutdown(int exitStrategy, int status, MprTicks timeout);

/**
    Cancel a shutdown request
    @description A graceful shutdown request initiated via #mprShutdown may be cancelled if the shutdown is still in progress
    and has not passed the point of no return. If the MPR is still in the MPR_STOPPING state, the shutdown may be cancelled.
    See #mprGetState.
    @return True if the shutdown can be cancelled. Returns false if a shutdown has not been requested or if the shutdown has
    advanced past the point of no return.
    @ingroup Mpr
    @stability Stable
 */
PUBLIC bool mprCancelShutdown(void);

#if ME_WIN_LIKE
/**
    Get the Windows window handle
    @return the windows HWND reference
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC HWND mprGetHwnd(void);

/**
    Get the windows application instance
    @return The application instance identifier
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC HINSTANCE mprGetInst(void);

/**
    Set the MPR windows handle
    @param handle Set the MPR default windows handle
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetHwnd(HWND handle);

/**
    Set the windows application instance
    @param inst The new windows application instance to set
    @ingroup Mpr
    @stability Stable.
 */
PUBLIC void mprSetInst(HINSTANCE inst);

/**
    Set the socket message number.
    @description Set the socket message number to use when using WSAAsyncSelect for windows.
    @param message Message number to use.
    @ingroup Mpr
    @stability Stable.
  */
PUBLIC void mprSetSocketMessage(int message);
#endif

#if ME_WIN_LIKE || CYGWIN
/**
    List the subkeys for a key in the Windows registry
    @param key Windows registry key to enumerate subkeys
    @return List of subkey string names
    @ingroup Mpr
    @stability Stable
  */
PUBLIC MprList *mprListRegistry(cchar *key);

/**
    Read a key from the Windows registry
    @param key Windows registry key to read
    @param name Windows registry name to read.
    @return The key/name setting
    @ingroup Mpr
    @stability Stable.
  */
PUBLIC char *mprReadRegistry(cchar *key, cchar *name);

/**
    Write a key value the Windows registry
    @param key Windows registry key to write
    @param name Windows registry name to write.
    @param value Value to set the key/name to.
    @return Zero if successful. Otherwise return a negative MPR error code.
    @ingroup Mpr
    @stability Stable.
  */
PUBLIC int mprWriteRegistry(cchar *key, cchar *name, cchar *value);
#endif /* ME_WIN_LIKE || CYGWIN */

#if VXWORKS
    PUBLIC int mprFindVxSym(SYMTAB_ID sid, char *name, char **pvalue);
    PUBLIC pid_t mprGetPid(void);
    #ifndef getpid
        #define getpid mprGetPid
    #endif
#endif

/*
    Internal
 */
PUBLIC void mprWriteToOsLog(cchar *msg, int level);

#ifdef __cplusplus
}
#endif
#endif /* _h_MPR */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
