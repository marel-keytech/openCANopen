/**
    @defgroup CanIOMapDriver

    Handles message mapping between CAN and Pluto Pins

    @file CanIOHandlerInterface.h

    Describes the interface of the handler

    The driver interface types are being declared here.
    The handler interface is also being declared here but the implementation
    is left to the subclass of the handler interface.

    @author Einar M. Bjorgvinsson <einar.mar.bjorgvinsson@marel.com>

    <b>
    Copyright (c) 2009 Marel Food Systems
    Proprietary material, all rights reserved, use or disclosure forbidden.
    </b>

    @ingroup CanIOMapDriver
 */


/* do not double include this file */
#ifndef _can_handler_interface__h
#define _can_handler_interface__h

/*!
 TODO: Check if this needs to be re-defined
 */
#ifndef M_UU
#define M_UU __attribute__((unused))
#endif

/**
 * Default path where the CAN <-> IO map descriptions are
 *
 * TODO Should be stored in the config
 */
#define CAN_IO_DEF_XML_PATH "/var/marel/canmaster/node_descriptions"
#define CAN_IO_DEF_CUSTOM_XML_PATH "/var/marel/canmaster/custom_node_descriptions"

//--------------------------------------------------------------------
// includes
//--------------------------------------------------------------------
#include "CanMasterInterface.h"
#include "canopen.h"
#include "osa.h"

// defines --------------------------------------------------------------------
using namespace CanIOMapDriver;

namespace CanIOMapDriver
{

class CanIOHandlerInterface
{
    public:

        /**
        * @brief Constructor
        * @param nodeName The name of the CAN node
        * @param cmi Pointer reference to Can Master, used when sending data to CAN
        */
        CanIOHandlerInterface(){};
        virtual ~CanIOHandlerInterface(){};

        // Receive functions, called by the CANMaster when data from the CAN device has been received
        /**
         * @brief Called when there is a new PDO CAN message from channel 1
         * @param data The message
         * @param size The size of the message
         * @return 0 if successfull, 1 if nothing was done or -1 if an error occurred
         * @note Called by the CanMaster
         */
        virtual int processPdo1( unsigned char* data M_UU, size_t size M_UU ) = 0;
        /**
         * @brief Called when there is a new PDO CAN message from channel 2
         * @param data The message
         * @param size The size of the message
         * @return 0 if successfull, 1 if nothing was done or -1 if an error occurred
         * @note Called by the CanMaster
         */
        virtual int processPdo2( unsigned char* data M_UU, size_t size M_UU ) = 0;
        /**
         * @brief Called when there is a new PDO CAN message from channel 3
         * @param data The message
         * @param size The size of the message
         * @return 0 if successfull, 1 if nothing was done or -1 if an error occurred
         * @note Called by the CanMaster
         */
        virtual int processPdo3( unsigned char* data M_UU, size_t size M_UU ) = 0;
        /**
         * @brief Called when there is a new PDO CAN message from channel 4
         * @param data The message
         * @param size The size of the message
         * @return 0 if successfull, 1 if nothing was done or -1 if an error occurred
         * @note Called by the CanMaster
         */
        virtual int processPdo4( unsigned char* data M_UU, size_t size M_UU ) = 0;
        /**
         * @brief Called when there is a new SDO CAN message
         * @param data The message
         * @param size The size of the message
         * @return 0 if successfull, 1 if nothing was done or -1 if an error occurred
         * @note Called by the CanMaster
         */
        virtual int processSdo( int index M_UU,
                                int subindex M_UU,
                                unsigned char *data M_UU,
                                size_t size M_UU ) = 0;
        /**
         * @brief Called when there is a new Emergency CAN message
         * @param emergencyErrorcode The emergency code number
         * @param errorRegister The registry number of the error
         * @param manufacturerError Manufacturer specific error number
         * @return 0 if successfull, 1 if nothing was done or -1 if an error occurred
         * @note Called by the CanMaster
         */
        virtual int processEmr(unsigned short emergencyErrorcode M_UU,
                               unsigned char errorRegister M_UU,
                               unsigned long long manufacturerError M_UU) = 0;
        /**
         * @brief Called when there is a new State CAN message
         * @param state The new CAN state
         * @return 0 if successfull, 1 if nothing was done or -1 if an error occurred
         * @note Called by the CanMaster
         */
        virtual int processNodeState(int state M_UU) = 0;

        /**
         * Initializes the handler
         * @return 0 if successful, 1 if successful but with errors and -1 if failed
         */
         virtual int initialize() = 0;
};


} /* End of namespace */

//--------------------------------------------------------------------
// Driver interface declarations
//--------------------------------------------------------------------


#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

    /**
     * Array of profiles that the driver supports, terminated with -1
     *
     * @ingroup CANDriverInterface
     */
    extern const int candriver_profile[];
    /**
     * Denotes the driver version.
     *
     * The driver implementer must set this version variable so
     * it can be loaded from the driver itself
     *
     * @ingroup CANDriverInterface
     */
    extern const int candriver_version;
    /**
     * Sets owner and instance information which are static for each driver
     *
     * @param owner The name of the owner (program name)
     * @param instance The instance ID
     * @return 0 if successful else -1
     * @ingroup CANDriverInterface
     */
    extern int candriver_set_parent(const char* owner, int instance);
    /**
     * Require a new handler for a node
     *
     * The driver checks if he supports the node/profile and returns
     * accrodingly to that.  He must check for both the nodeName and the
     * profile number.
     *
     * @param nodeName The name of the node that follows this new handler
     * @pararm profileNr The node profile number
     * @param cmi CanMasterInterface used for callbacks
     * @param chi The CanHandlerInterface being set
     * @return 0 if successful, -1 if unable to get handler interface, -2 if the master interface is invalid
     * @ingroup CANDriverInterface
     */
    extern int candriver_create_handler(const char* nodeName, int32_t profileNr,
                                        CanIOMapDriver::CanMasterInterface *cmi,
                                        CanIOMapDriver::CanIOHandlerInterface** chi );
    /**
     * Clean up.
     * Used by the driver loader when deleting a specific handler
     *
     * @param chi The handler to delete
     * @return 0 if successful else -1
     * @ingroup CANDriverInterface
     */
    extern int candriver_delete_handler( CanIOMapDriver::CanIOHandlerInterface *chi );
    /**
     * Deletes all handlers
     *
     * @return 0 if successful else -1
     * @ingroup CANDriverInterface
     */
    extern int candriver_delete_all_handlers();
    /**
     * Driver clean up when closing the driver
     *
     * @return 0 is sucessful else -1
     * @ingroup CANDriverInterface
     */
    extern int candriver_close_driver();

    /**
     * @typedef candriver_set_parent_t
     *
     * Used by the driver loader when loading the functions (symbols)
     * from this driver and assigning the function pointers locally
     *
     * @ingroup CANDriverInterface
     */
    typedef int candriver_set_parent_t(const char* owner, int instance);
    /**
     * @typedef candriver_set_parent_t
     *
     * Used by the driver loader when loading the functions (symbols)
     * from this driver and assigning the function pointers locally
     *
     * @ingroup CANDriverInterface
     */
    typedef int candriver_create_handler_t( const char* nodeName, int32_t profileNr,
                                            CanIOMapDriver::CanMasterInterface *cmi,
                                            CanIOMapDriver::CanIOHandlerInterface** chi);
    /**
     * @typedef candriver_delete_handler_t
     *
     * Used by the driver loader when loading the functions (symbols)
     * from this driver and assigning the function pointers locally
     *
     * @ingroup CANDriverInterface
     */
    typedef int candriver_delete_handler_t(CanIOMapDriver::CanIOHandlerInterface *chi);
    /**
     * @typedef candriver_set_parent_t
     *
     * Used by the driver loader when loading the functions (symbols)
     * from this driver and assigning the function pointers locally
     *
     * @ingroup CANDriverInterface
     */
    typedef int candriver_delete_all_handlers_t();
    /**
     * @typedef candriver_set_parent_t
     *
     * Used by the driver loader when loading the functions (symbols)
     * from this driver and assigning the function pointers locally
     *
     * @ingroup CANDriverInterface
     */
    typedef int candriver_close_driver_t();
    /**
     * @typedef candriver_set_parent_t
     *
     * Used by the driver loader when loading the functions (symbols)
     * from this driver and assigning the function pointers locally
     *
     * @ingroup CANDriverInterface
     */

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif


#endif /* _can_handler_interface__h */
