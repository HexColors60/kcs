/****************************************************************************/
/*!
 * @file  timer.hpp
 * @brief timer �N���X
 *
 *    ���s���Ԍv�������܂�.
 *
 * @author K.Nagai.
 *
 * @date 2008/08/08 K.Nagai Re-write by a new coding style guide.
 *
 * Copyright (C) 2008 K.Nagai
 */
/****************************************************************************/
#ifndef KLIB_TIMER_HPP_
#define KLIB_TIMER_HPP_

#if defined(_WIN32) || defined(_WIN64)
#ifdef _MSC_VER
#pragma warning( disable: 4786 )
#pragma warning( disable: 4503 )
#endif // _MSC_VER
#endif

#include <iostream>

#if defined(KLIB_USE_DEFAULT_TIMER)

    #include <time.h>

    #define KLIB_TMR_DEFINE_CLOCK_MEMBER()  clock_t m_start
    #define KLIB_TMR_DECL_CURRENT_DATAVAR() clock_t cur
    #define KLIB_TMR_SET_START_TIME()       m_start = clock()
    #define KLIB_TMR_GET_EXECUTE_TIME()     (((double)cur-(double)m_start)/CLOCKS_PER_SEC)
    #define KLIB_TMR_SET_CURRENT_TIME()     cur = clock()

#elif defined(_WIN32) || defined(_WIN64)

    #include <windows.h>
    #define KLIB_TMR_DEFINE_CLOCK_MEMBER()  mutable LARGE_INTEGER m_start, m_freq
    #define KLIB_TMR_DECL_CURRENT_DATAVAR() LARGE_INTEGER cur
    #define KLIB_TMR_SET_START_TIME()       QueryPerformanceFrequency(&m_freq),QueryPerformanceCounter(&m_start)
    #define KLIB_TMR_GET_EXECUTE_TIME()     (((double)cur.QuadPart-(double)m_start.QuadPart)/(double)m_freq.QuadPart)
    #define KLIB_TMR_SET_CURRENT_TIME()     QueryPerformanceCounter(&cur)

#else

    #include <sys/time.h>
    #include <time.h>
    #include <unistd.h>

    #if defined(KLIB_TMR_USE_CLOCK_GETTIME) && defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)

        #define KLIB_TMR_DEFINE_CLOCK_MEMBER()  mutable struct timespec m_start
        #define KLIB_TMR_DECL_CURRENT_DATAVAR() struct timespec cur
        #define KLIB_TMR_SET_START_TIME()       clock_gettime(CLOCK_MONOTONIC, &m_start)
        #define KLIB_TMR_MAKE_TIME(x)           (((double)(x).tv_sec)+((double)(x).tv_nsec)/1000000000)
        #define KLIB_TMR_GET_EXECUTE_TIME()     (KLIB_TMR_MAKE_TIME(cur)-KLIB_TMR_MAKE_TIME(m_start))
        #define KLIB_TMR_SET_CURRENT_TIME()     clock_gettime(CLOCK_MONOTONIC, &cur)

    #else

        #define KLIB_TMR_DEFINE_CLOCK_MEMBER()  mutable struct timeval m_start
        #define KLIB_TMR_DECL_CURRENT_DATAVAR() struct timeval cur
        #define KLIB_TMR_SET_START_TIME()       gettimeofday(&m_start, NULL)
        #define KLIB_TMR_MAKE_TIME(x)           ((double)(x).tv_sec+((double)((x).tv_usec)/1000000))
        #define KLIB_TMR_GET_EXECUTE_TIME()     (KLIB_TMR_MAKE_TIME(cur)-KLIB_TMR_MAKE_TIME(m_start))
        #define KLIB_TMR_SET_CURRENT_TIME()     gettimeofday(&cur, NULL)

    #endif

#endif

namespace klib {

    //////////////////////////////////////////////////////////////////////////
    /*!
     * @class timer timer.hpp
     * @brief timer �N���X.
     *
     *        ���쎞�Ԃ��v������N���X�ł�.
     *        �R���X�g���N�g���ꂽ��������̌o�ߎ��Ԃ� elapsed() �����o�֐���
     *        �擾���܂�. �擾�����l�� double �^�ŁA�P�ʂ� '�b' �ł�.
     *
     * @author K.Nagai
     * @date 2008/08/08
     */
    //////////////////////////////////////////////////////////////////////////
    class timer
    {
    public:
        //////////////////////////////////////////////////////////////////////
        /*!
         * @brief �R���X�g���N�^
         *
         *        timer �N���X�̃R���X�g���N�^�ł�.
         *        �R���X�g���N�g���̌��ݎ�����ێ����܂�.
         *
         * @author K.Nagai
         * @date 2008/08/08
         */
        //////////////////////////////////////////////////////////////////////
        timer()
        {
            KLIB_TMR_SET_START_TIME();
        }

        //////////////////////////////////////////////////////////////////////
        /*!
         * @brief �f�X�g���N�^
         *
         *        timer �N���X�̃f�X�g���N�^�ł�.
         *
         * @author K.Nagai
         * @date 2008/08/08
         */
        //////////////////////////////////////////////////////////////////////
        ~timer() throw() {}

        //////////////////////////////////////////////////////////////////////
        /*!
         * @brief �v���̍ăX�^�[�g.
         *
         *        �ێ����Ă��鎞�����Đݒ肷�邱�ƂŁA�v�����ăX�^�[�g�����܂�.
         *
         * @throw �Ȃ�
         *
         * @author K.Nagai
         * @date 2008/08/08
         */
        //////////////////////////////////////////////////////////////////////
        void restart()
        {
            KLIB_TMR_SET_START_TIME();
        }

        //////////////////////////////////////////////////////////////////////
        /*!
         * @brief �o�ߎ��Ԃ̎擾.
         *
         *        ���ݎ����ƕێ����Ă��鎞���̍�����b�P�ʂŕԂ��܂�.
         *
         * @return �o�ߎ��Ԃ�b�P�ʂŕԂ��܂�.
         *
         * @throw �Ȃ�
         *
         * @author K.Nagai
         * @date 2008/08/08
         */
        //////////////////////////////////////////////////////////////////////
        double elapsed() const
        {
            KLIB_TMR_DECL_CURRENT_DATAVAR();
            KLIB_TMR_SET_CURRENT_TIME();
            return (KLIB_TMR_GET_EXECUTE_TIME());
        }

        //////////////////////////////////////////////////////////////////////
        /*!
         * @brief �o�ߎ��Ԃ̏o��.
         *
         *        �o�ߎ��Ԃ� std::ostream �ɗ����܂�.
         *
         * @return �^����ꂽ std::ostream& �����̂܂ܕԂ��܂�.
         *
         * @throw �Ȃ�
         *
         * @author K.Nagai
         * @date 2008/08/08
         */
        //////////////////////////////////////////////////////////////////////
        friend std::ostream& operator<<(std::ostream& strm, const timer& tmr)
        {
            strm << tmr.elapsed();
            return strm;
        }

    private:
        KLIB_TMR_DEFINE_CLOCK_MEMBER(); // �����o�ϐ��̒�`.

        //////////////////////////////////////////////////////////////////////
        // ����͋�����܂���.
        //////////////////////////////////////////////////////////////////////
        timer(const timer&);
        timer& operator=(const timer&);
    };

} // namespace klib

#endif // KLIB_TIMER_HPP_
