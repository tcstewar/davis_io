#include <libcaer/libcaer.h>
#include <libcaer/devices/davis.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>

static atomic_bool globalShutdown = ATOMIC_VAR_INIT(false);

static void globalShutdownSignalHandler(int signal) {
	// Simply set the running flag to false on SIGTERM and SIGINT (CTRL+C) for global shutdown.
	if (signal == SIGTERM || signal == SIGINT) {
		atomic_store(&globalShutdown, true);
	}
}

static void usbShutdownHandler(void *ptr) {
	(void) (ptr); // UNUSED.

	atomic_store(&globalShutdown, true);
}

int main(int argc, char **argv) {


    if ((argc != 3) && (argc!=2)) {
        printf("Must specify one or two file names\n");
        return (EXIT_FAILURE);
    }
    
    FILE *file_events = fopen(argv[1], "wb");    
    FILE *file_frames = NULL;
    if (argc == 3) {
        file_frames = fopen(argv[2], "wb");
    }


// Install signal handler for global shutdown.
#if defined(_WIN32)
	if (signal(SIGTERM, &globalShutdownSignalHandler) == SIG_ERR) {
		caerLog(CAER_LOG_CRITICAL, "ShutdownAction", "Failed to set signal handler for SIGTERM. Error: %d.", errno);
		return (EXIT_FAILURE);
	}

	if (signal(SIGINT, &globalShutdownSignalHandler) == SIG_ERR) {
		caerLog(CAER_LOG_CRITICAL, "ShutdownAction", "Failed to set signal handler for SIGINT. Error: %d.", errno);
		return (EXIT_FAILURE);
	}
#else
	struct sigaction shutdownAction;

	shutdownAction.sa_handler = &globalShutdownSignalHandler;
	shutdownAction.sa_flags   = 0;
	sigemptyset(&shutdownAction.sa_mask);
	sigaddset(&shutdownAction.sa_mask, SIGTERM);
	sigaddset(&shutdownAction.sa_mask, SIGINT);

	if (sigaction(SIGTERM, &shutdownAction, NULL) == -1) {
		caerLog(CAER_LOG_CRITICAL, "ShutdownAction", "Failed to set signal handler for SIGTERM. Error: %d.", errno);
		return (EXIT_FAILURE);
	}

	if (sigaction(SIGINT, &shutdownAction, NULL) == -1) {
		caerLog(CAER_LOG_CRITICAL, "ShutdownAction", "Failed to set signal handler for SIGINT. Error: %d.", errno);
		return (EXIT_FAILURE);
	}
#endif

	// Open a DAVIS, give it a device ID of 1, and don't care about USB bus or SN restrictions.
	caerDeviceHandle davis_handle = caerDeviceOpen(1, CAER_DEVICE_DAVIS, 0, 0, NULL);
	if (davis_handle == NULL) {
		return (EXIT_FAILURE);
	}

	// Let's take a look at the information we have on the device.
	struct caer_davis_info davis_info = caerDavisInfoGet(davis_handle);

	printf("%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n", davis_info.deviceString,
		davis_info.deviceID, davis_info.deviceIsMaster, davis_info.dvsSizeX, davis_info.dvsSizeY,
		davis_info.logicVersion);

	// Send the default configuration before using the device.
	// No configuration is sent automatically!
	caerDeviceSendDefaultConfig(davis_handle);

    /*
	// Tweak some biases, to increase bandwidth in this case.
	struct caer_bias_coarsefine coarseFineBias;

	coarseFineBias.coarseValue        = 2;
	coarseFineBias.fineValue          = 116;
	coarseFineBias.enabled            = true;
	coarseFineBias.sexN               = false;
	coarseFineBias.typeNormal         = true;
	coarseFineBias.currentLevelNormal = true;
	caerDeviceConfigSet(
		davis_handle, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP, caerBiasCoarseFineGenerate(coarseFineBias));

	coarseFineBias.coarseValue        = 1;
	coarseFineBias.fineValue          = 33;
	coarseFineBias.enabled            = true;
	coarseFineBias.sexN               = false;
	coarseFineBias.typeNormal         = true;
	coarseFineBias.currentLevelNormal = true;
	caerDeviceConfigSet(
		davis_handle, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP, caerBiasCoarseFineGenerate(coarseFineBias));

	// Let's verify they really changed!
	uint32_t prBias, prsfBias;
	caerDeviceConfigGet(davis_handle, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP, &prBias);
	caerDeviceConfigGet(davis_handle, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP, &prsfBias);

	printf("New bias values --- PR-coarse: %d, PR-fine: %d, PRSF-coarse: %d, PRSF-fine: %d.\n",
		caerBiasCoarseFineParse(prBias).coarseValue, caerBiasCoarseFineParse(prBias).fineValue,
		caerBiasCoarseFineParse(prsfBias).coarseValue, caerBiasCoarseFineParse(prsfBias).fineValue);
    */
	// Now let's get start getting some data from the device. We just loop in blocking mode,
	// no notification needed regarding new events. The shutdown notification, for example if
	// the device is disconnected, should be listened to.
	caerDeviceDataStart(davis_handle, NULL, NULL, NULL, &usbShutdownHandler, NULL);

	// Let's turn on blocking data-get mode to avoid wasting resources.
	caerDeviceConfigSet(davis_handle, CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true);
    
    //caerDeviceConfigSet(davis_handle, DAVIS_CONFIG_USB, DAVIS_CONFIG_USB_EARLY_PACKET_DELAY, 1);
    
    caerDeviceConfigSet(davis_handle, DAVIS_CONFIG_DVS, DAVIS_CONFIG_DVS_RUN, true);
    caerDeviceConfigSet(davis_handle, DAVIS_CONFIG_APS, DAVIS_CONFIG_APS_RUN, true);
	caerDeviceConfigSet(davis_handle, DAVIS_CONFIG_IMU, DAVIS_CONFIG_IMU_RUN_ACCELEROMETER, false);
	caerDeviceConfigSet(davis_handle, DAVIS_CONFIG_IMU, DAVIS_CONFIG_IMU_RUN_GYROSCOPE, false);
	caerDeviceConfigSet(davis_handle, DAVIS_CONFIG_IMU, DAVIS_CONFIG_IMU_RUN_TEMPERATURE, false);
	caerDeviceConfigSet(davis_handle, DAVIS_CONFIG_EXTINPUT, DAVIS_CONFIG_EXTINPUT_RUN_DETECTOR, false);    

    caerDeviceConfigSet(davis_handle, DAVIS_CONFIG_APS, DAVIS_CONFIG_APS_AUTOEXPOSURE, true);
    
    
    uint32_t packet_count = 0;
    uint32_t frame_count = 0;
    int32_t start_time = 0;
    
	while (!atomic_load_explicit(&globalShutdown, memory_order_relaxed)) {
		caerEventPacketContainer packetContainer = caerDeviceDataGet(davis_handle);
		if (packetContainer == NULL) {
			continue; // Skip if nothing there.
		}

		int32_t packetNum = caerEventPacketContainerGetEventPacketsNumber(packetContainer);

		//printf("\nGot event container with %d packets (allocated).\n", packetNum);

		for (int32_t i = 0; i < packetNum; i++) {
			caerEventPacketHeader packetHeader = caerEventPacketContainerGetEventPacket(packetContainer, i);
			if (packetHeader == NULL) {
				//printf("Packet %d is empty (not present).\n", i);
				continue; // Skip if nothing there.
			}

			//printf("Packet %d of type %d -> size is %d.\n", i, caerEventPacketHeaderGetEventType(packetHeader),
			//	caerEventPacketHeaderGetEventNumber(packetHeader));

			if (i == POLARITY_EVENT) {
				caerPolarityEventPacket polarity = (caerPolarityEventPacket) packetHeader;
				
				
				caerPolarityEventConst firstEvent = caerPolarityEventPacketGetEventConst(polarity, 0);
				int32_t ts = caerPolarityEventGetTimestamp(firstEvent);
				if (ts > start_time + 1000000) {
				    printf("packet rate: %d Hz     frame rate: %d Hz\n", packet_count, frame_count);
				    packet_count = 0;
				    frame_count = 0;
				    start_time += 1000000;
				}
				    
				
				packet_count += 1;
				    
				
				fprintf(stderr, "\r%12d                ", ts);
				
				int32_t count = caerEventPacketHeaderGetEventNumber(&polarity->packetHeader);
				
				for (int i=0; i<count; i++) {
				    caerPolarityEventConst event = caerPolarityEventPacketGetEventConst(polarity, i);
				    fwrite(event, 8, 1, file_events);
				}
				
				/*

				// Get full timestamp and addresses of first event.
				caerPolarityEventConst firstEvent = caerPolarityEventPacketGetEventConst(polarity, 0);

				int32_t ts = caerPolarityEventGetTimestamp(firstEvent);
				uint16_t x = caerPolarityEventGetX(firstEvent);
				uint16_t y = caerPolarityEventGetY(firstEvent);
				bool pol   = caerPolarityEventGetPolarity(firstEvent);

				printf("First polarity event - ts: %d, x: %d, y: %d, pol: %d.\n", ts, x, y, pol);
				*/
			}

			if (i == FRAME_EVENT) {
				caerFrameEventPacket frame = (caerFrameEventPacket) packetHeader;

				int32_t count = caerEventPacketHeaderGetEventNumber(&frame->packetHeader);
				//printf("count:%d\n", count);
				
				for (int i=0; i<count; i++) {
    				caerFrameEventConst event = caerFrameEventPacketGetEventConst(frame, i);
    				
    				frame_count += 1;

				    int32_t ts   = caerFrameEventGetTimestamp(event);
				    if (file_frames != NULL) {
    				    fwrite(&ts, 4, 1, file_frames);
    				    //fwrite(event->pixels, 2, 240*180, file_frames);
    				    //fwrite(event->pixels, 2, 6, file_frames);
                        //int sy = 180; //caerFrameEventGetLengthY(event);
                        //int sx = 240; //caerFrameEventGetLengthX(event);
                        //printf("%dx%d\n", sy, sx);
                        
                        
				        for (int32_t y = 0; y < 180; y++) {
					        for (int32_t x = 0; x < 240; x++) {
				        //for (int32_t y = 90; y < 91; y++) {
					    //    for (int32_t x = 120; x < 126; x++) {
				       		    uint16_t v = caerFrameEventGetPixel(event, x, y);
				       		    fwrite(&v, 2, 1, file_frames);
					        }
				        }
				        
    				}
				    

                    

                    
				    
				}

				//printf("First frame event - ts: %d, sum: %" PRIu64 ".\n", ts, sum);
			}
		}

		caerEventPacketContainerFree(packetContainer);
	}

    fflush(file_events);
    fclose(file_events);	
    if (file_frames != NULL) {
        fflush(file_frames);
        fclose(file_frames);	
    }

	caerDeviceDataStop(davis_handle);

	caerDeviceClose(&davis_handle);

	printf("Shutdown successful.\n");

	return (EXIT_SUCCESS);
}
