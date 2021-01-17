#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <semaphore.h>
#include <stdlib.h>
#include "dvb_t.h"

#ifdef __USE_EXPRESS__

#include "express.h"

static struct usb_device    *m_usbdev;
static unsigned int          m_xfrs_in_progress;
static int                   m_express_status;
static int                   m_sample_block_size;
static bool                  m_si570_fitted;
static libusb_device_handle *m_handle;
static libusb_context       *m_usbctx;
static sem_t usb_sem;
static int m_tx_hardware;
static unsigned char         m_ancillary;

#define EXPRESS_DATA 1
#define EXPRESS_I2C  2


const char *express_firmware_get_path( const char *filename, char *pathname )
{
    sprintf( pathname,"/lib/firmware/datvexpress/%s", filename );
    return pathname;
}

//
// Transfer buffer handling
//

libusb_transfer *express_alloc_transfer(void)
{
    m_xfrs_in_progress++;
    return libusb_alloc_transfer(0);
}
void express_free_transfer( libusb_transfer *xfr  )
{
    if(m_xfrs_in_progress > 0 )m_xfrs_in_progress--;
    libusb_free_transfer( xfr );
}
void express_handle_events(int n)
{
    for( int i = 0; i < n; i++  )
    {
        libusb_handle_events(m_usbctx);
    }
}

//
// This call back handles data transfers
//
void cb_xfr( struct libusb_transfer *xfr )
{
    if( xfr->status == LIBUSB_TRANSFER_COMPLETED )
    {
        //uint8_t *b  = (uint8_t*)xfr->buffer;
        int   *id = (int*)xfr->user_data;
        if( *id == EXPRESS_DATA )
        {
            express_free_transfer( xfr );
        }
    }
}
//
// This callback handles I2C messages
//
void cb_i2c( struct libusb_transfer *xfr )
{
    if( xfr->status == LIBUSB_TRANSFER_COMPLETED )
    {
        // response buffer
        unsigned char *b = (unsigned char*)xfr->buffer;
        int *id = (int*)xfr->user_data;

        // Figure what response we are getting
        switch(b[0])
        {
        case (ADRF6755_ADD | I2C_RD):
           // if((xfr->actual_length == 4)&&(b[2] == 33))
           //     loggerf("ADRF6755 version %d\n",b[3]);
            break;
        case (AD7992_ADD | I2C_RD):
            {
                if(xfr->actual_length == 7)
                {
                    short chan0 = b[3];
                    chan0 = (chan0<<8) | b[4];
                    short chan1 = b[5];
                    chan1 = (chan1<<8) | b[6];
//                    loggerf("ADC samples %.4X %.4X\n", chan0, chan1);
                }
            }
            break;
        case (FLASH_ADD | I2C_RD):
            {
                if(xfr->actual_length == b[1]+4 )
                {
//                    loggerf("FLASH: %.2X%.2X ",b[2],b[3]);
//                    for( int i = 0; i < b[1]; i++ )
//                        loggerf(" %.2X",b[i+4]);
//                    loggerf("\n");
                }
            }
        case (SI570_ADD | I2C_RD):
//            if((xfr->actual_length == 4)&&(b[2]!=0)) si570_rx(b[2], b[3]);
            break;
        default:
            break;
        }
        if(*id == EXPRESS_I2C) express_free_transfer( xfr );
    }
    else
    {
        //loggerf("I2C transfer failed callback\n");
    }
}
void express_wait( void )
{
    struct timespec tim;

    tim.tv_sec  = 0;
    tim.tv_nsec = 100000000; // 100 ms
    nanosleep( &tim, NULL);
}
void express_wait_long( void )
{
    struct timespec tim;

    tim.tv_sec  = 1; // 1 sec
    tim.tv_nsec = 0;
    nanosleep( &tim, NULL);
}
//
// Used to send data
//
int express_bulk_transfer(int ep, unsigned char *b, int l )
{
    int res;
    static int id = EXPRESS_DATA;
    struct libusb_transfer *xfr = express_alloc_transfer();
    libusb_fill_bulk_transfer( xfr, m_handle, ep, b, l, cb_xfr, &id, USB_TIMEOUT );
    if((res=libusb_submit_transfer( xfr )) < 0 )
    {
        express_free_transfer( xfr );
    }
    return res;
}
//
// Used to send I2C messages
//
int express_i2c_bulk_transfer(int ep, unsigned char *b, int l )
{
    int res;
    static int id = EXPRESS_I2C;
    struct libusb_transfer *xfr = express_alloc_transfer();
    libusb_fill_bulk_transfer( xfr, m_handle,ep, b, l,cb_i2c, &id, USB_TIMEOUT );
    if((res=libusb_submit_transfer( xfr )) < 0 )
    {
        express_free_transfer( xfr );
        printf("I2C transfer failed\n");
    }
    return res;
}
int express_i2c_bulk_transfer_blocking(int ep, unsigned char *b, int l )
{
	int sbytes;
    return libusb_bulk_transfer(m_handle,ep,b,3,&sbytes,USB_TIMEOUT);

}
///////////////////////////////////////////////////////////////////////////////////////
//
// Only called at start up
//
///////////////////////////////////////////////////////////////////////////////////////

//
// Initialise the fx2 device
//
int express_find( void )
{
    // discover devices
    m_usbdev = NULL;
    int ret;

    if( libusb_init( &m_usbctx ) < 0 )
    {
        printf("Cannot init USB\n");
        return EXP_MISS;
    }
    //libusb_set_debug( m_usbctx,3 );
    // Bulk transfers on EP2 of samples
    // This is not the preferred way of doing this as it only shows the first
    // such device.
    if((m_handle = libusb_open_device_with_vid_pid( m_usbctx, USB_VENDOR, USB_PROD))==NULL)
    {
        return EXP_MISS;
    }
    if(libusb_kernel_driver_active(m_handle,0)) libusb_detach_kernel_driver(m_handle,0);

    if((ret=libusb_claim_interface(m_handle,0))<0)
    {
        printf("Cannot claim USB interface %d\n",ret);
        libusb_close(m_handle);
        libusb_exit(m_usbctx);
        return EXP_MISS;
    }
/*
    libusb_device_descriptor  desc;
    unsigned char data[255];
    if(libusb_get_descriptor(m_handle, LIBUSB_DT_DEVICE, 0, (unsigned char*)&desc, sizeof(libusb_device_descriptor) )>=0)
    {
        loggerf("D length %d\n",desc.bLength);
        loggerf("D Product id %.4x\n",desc.idProduct);
        loggerf("D Vendor id %.4x\n",desc.idVendor);
        loggerf("D Class %.1x\n",desc.bDeviceClass);
        loggerf("D Sub Class %.1x\n",desc.bDeviceSubClass);
        loggerf("D Max Packet size %d\n",desc.bMaxPacketSize0);
    }
*/
    if((ret=libusb_set_interface_alt_setting(m_handle,0,1))<0)
    {
        printf("Express cannot set interface alt %d\n",ret);
        libusb_release_interface(m_handle,0);
        libusb_attach_kernel_driver(m_handle,0);
        libusb_close(m_handle);
        libusb_exit(m_usbctx);
        return EXP_MISS;
    }
    //unsigned char text[256];
    //libusb_get_string_descriptor_ascii( m_handle, 0,text,256);
    //loggerf("Desc: %s\n",text);
    //libusb_device *dev = libusb_get_device(m_handle);
    //int speed =  libusb_get_device_speed(dev);
    //loggerf("Speed %d\n",speed);
    //    loggerf("Size %d\n",libusb_get_max_iso_packet_size(dev, EP2));
    //libusb_set_debug( m_usbctx,0 );
    return EXP_OK;
}

//
// Write a 1 to reset the FX2 device
//
void express_reset(void)
{
    unsigned char data = 1;
    if( m_express_status != EXP_OK ) return;
    int res = libusb_control_transfer(m_handle,0x40,0xa0,0xE600,0,&data,1,USB_TIMEOUT);
    if( res < 0) printf("Express HW reset failed\n");
}
//
// Write a 0 to start the FX2 running
//
void express_run(void)
{
    unsigned char data = 0;
    if( m_express_status != EXP_OK ) return;
    int res = libusb_control_transfer(m_handle,0x40,0xa0,0xE600,0,&data,1,USB_TIMEOUT);
    if( res < 0) printf("Express HW run failed\n");
}
//
// Load the FX2 Firmware
//
int express_firmware_load(const char *fx2_filename)
{
    FILE *fp;
    if((fp=fopen(fx2_filename,"r"))!=NULL)
    {
        int line = 0;
        const size_t buflen = 1024;
        char buffer[buflen];
        char *b;

        while(fgets(buffer,buflen,fp))
        {
            if(feof(fp)) break;
            line++;
            b = buffer;
            unsigned int nbytes=0,addr=0,type=0;
            if(b[0] == ':')
            {
                b++;
                sscanf(b,"%02x%04x%02x",&nbytes,&addr,&type);
                b += 8;
                unsigned int d;
                unsigned char data[nbytes];
                unsigned char chksum = nbytes+addr+(addr>>8)+type;
                for( unsigned int i = 0; i < nbytes; i++ )
                {
                    sscanf(b,"%02x",&d);
                    b += 2;
                    data[i] = d;
                    chksum += d;
                }
                unsigned int fchksum  = 0;
                sscanf(b,"%02x",&fchksum);
                if((chksum+fchksum)&0xFF)
                {
                    printf("Express Firmware file CRC error\n");
                }
                else
                {
                   // Write to RAM
                   if(libusb_control_transfer(m_handle,0x40,0xa0,addr,0,data,nbytes,USB_TIMEOUT)<0)
                   {
                       printf("Express HW firmware control transfer load failed\n");
                       break;
                   }
                }
            }
        }
        fclose(fp);
    }
    else
    {
        printf("Express cannot find firmware file %s\n",fx2_filename);
        return EXP_IHX;
    }
    return EXP_OK;
}
//
// Load the FPGA code
//
int express_fpga_load( const char *fpga_filename )
{
    FILE *fp;
    if((fp=fopen(fpga_filename,"r"))!=NULL)
    {
        const size_t buflen = 64;
        unsigned char buffer[buflen];
        int nbytes,sbytes;

        while((nbytes=fread(buffer,1,buflen,fp))!=0)
        {
           if(libusb_bulk_transfer(m_handle,EP1OUT,buffer,nbytes,&sbytes,USB_TIMEOUT)<0)
           {
               printf("Express HW FPGA Fbulk write failed\n");
               break;
           }
           if(nbytes != sbytes) printf("FPGA transfer mismatch\n");
        }
        fclose(fp);
    }
    else
    {
        printf("Express HW cannot find FPGA file %s\n",fpga_filename);
        return EXP_RBF;
    }
    return EXP_OK;
}
//
// These could be done in a single transaction but as speed is not
// of prime importance they are not being done that way.
//
void express_configure_adrf6755( void )
{
    unsigned char b[3];
    b[0]  = ADRF6755_ADD | I2C_WR;//address

     // set attenuator gain to 47db
     b[1] = 30;b[2]=0x3F;
//     if(libusb_bulk_transfer(m_handle,EP1,b,3,&sbytes,USB_TIMEOUT)< 0)printf("Bulk write failed %d\n",__LINE__);

     // power down the modulator
     b[1]=29;b[2]=0x80;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // power up internal VCO, 1.3 GHz
     b[1]=28;b[2]=0x08;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
     //
     b[1]=27;b[2]=0x10;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // reserved registers
     b[1]=26;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     b[1]=25;b[2]=0x64;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // enable autocalibration
     b[1]=24;b[2]=0x18;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // lock detect
     b[1]=23;b[2]=0x70;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // reserved registers
     b[1]=22;b[2]=0x80;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
     b[1]=21;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
     b[1]=20;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
     b[1]=19;b[2]=0x80;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
     b[1]=18;b[2]=0x60;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
     b[1]=17;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
     b[1]=16;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
     b[1]=15;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // enable attenuator
     b[1] = 14;b[2]=0x80;//1b
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
 
     //
     b[1]=13;b[2]=0xE8;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // power up pll
     b[1]=12;b[2]=0x18;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // reserved
     b[1]=11;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     //
     b[1] = 10;b[2]=0x21;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // charge pump
     // CR9
     b[1]=9;b[2]=0xF0;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // reserved
     b[1]=8;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // Set frequency to 1300 MHz ?
     // CR7
     b[1]=7;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // CR6 ?
     b[1]=6;b[2]=0x20;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // CR5 Disable R divider
     b[1]=5;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // CR4 reserved
     b[1]=4;b[2]=0x01;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // CR3
     b[1]=3;b[2]=0x05;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // CR2
     b[1]=2;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // CR1
     b[1]=1;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // CR0
     b[1]=0;b[2]=0x00;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

         // power lo monitor
     b[1]=27;b[2]=0x10;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );

     // wait for PLL lock
     // power up the modulator
     b[1]=29;b[2]=0x81;
     express_i2c_bulk_transfer_blocking( EP1OUT, b, 3 );
}
void express_read_adrf6755_version(void)
{
    if( m_express_status != EXP_OK ) return;
    // Format up into an I2C message for the ADRF6755
    unsigned char msg[4];
    msg[0]  = ADRF6755_ADD | I2C_RD;//address
    msg[1]  = 1;//1 byte to read
    msg[2]  = 33;//CR33
    msg[3]  = 0;//Response will go here
    // Set reg address to read from
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );
    // Read the actual register
    express_i2c_bulk_transfer( EP1IN, msg, 4 );

    express_handle_events( 2 );
}
//
// Code to configure the AD7992 ADC
//
void express_configure_ad7992(void)
{
    if( m_express_status != EXP_OK ) return;
    // Format up into an I2C message for the ADRF6755
    unsigned char msg[3];
    msg[0]  = AD7992_ADD | I2C_WR;//address
    msg[1]  = 0x02;//Configuration register
    msg[2]  = 0x30;//2 channel alternate no alert
    // Set address
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    express_handle_events( 1 );
}

//////////////////////////////////////////////////////////////////////////
//
// Called when system is running
//
//////////////////////////////////////////////////////////////////////////

//
// ARDF6755 Set frequency
//
void express_set_freq( double freq )
{
    double lo;
    unsigned char CR27 = 0;
    unsigned char CR28 = 0;

    if( m_express_status != EXP_OK ) return;

    if( freq > 1155000000 )
    {
        CR28 = 0x00;
        CR27 = 0x10;
        lo = freq;
    }
    else
    {
        if( freq > 577500000)
        {
            CR28 = 0x01;
            CR27 = 0x00;
            lo = freq*2;
        }
        else
        {
            if( freq > 288750000 )
            {
                CR28 = 0x02;
                CR27 = 0x00;
                lo   = freq*4;
            }
            else
            {
                if( freq > 144375000 )
                {
                    CR28 = 0x03;
                    CR27 = 0x00;
                    lo = freq*8;
                }
                else
                {
                    CR28 = 0x04;
                    CR27 = 0x00;
                    lo = freq*16;
                }
            }
        }
    }
    CR27 = CR27 | 0x00;
    CR28 = CR28 | 0x08;//b3=1

    // Divide by the reference frequency Xtal * 2
    double dintp,dfpart;
    dfpart = modf(lo/PFD40,&dintp);
    int ipart = dintp;
    // multiply the fractional part by 2^25
    long fpart = dfpart*33554432.0;
    // Format up into an I2C message for the ADRF6755
    unsigned char msg[3];

    msg[0] = ADRF6755_ADD | I2C_WR;//address
    // Now send it over EP1, should add error checking!
    // The FX2 firmware will send it over I2C

    msg[1] = 28;//CR28
    msg[2] = CR28;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1] = 7;//CR7
    msg[2] = (ipart>>8)&0x0F;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1] = 6;//CR6
    msg[2] = ipart&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1] = 3;//CR3
    msg[2] = 0x04 | ((fpart>>24)&0x01);
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1] = 2;//CR2
    msg[2] = (fpart>>16)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1] = 1;//CR1
    msg[2] = (fpart>>8)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1] = 0;//CR0
    msg[2] = fpart&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1] = 27;//CR27
    msg[2] = CR27;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    express_handle_events( 8 );
}
//
// Routines that affect the FPGA registers
//
//
// Set 1 of 4 filters in the FPGA
// When in TS mode sets the DVB-S FEC rate.
//
void express_set_interp( int interp )
{
    unsigned char msg[3];

    if( m_express_status != EXP_OK ) return;

    // Send it
    msg[0] = FPGA_ADD | I2C_WR;
    // MSB
    msg[1] = FPGA_INT_REG;
    msg[2] = interp;

    express_i2c_bulk_transfer( EP1OUT, msg, 3 );
    express_handle_events( 1 );
}

//
// Routines that affect the FPGA registers
//
int express_set_sr( double sr )
{
    long long val;
	double clk;
	int irate;
	long sr_threshold;
    unsigned char msg[3];
//    printcon("Symbol rate %.2f\n",sr);

    if( sr == 0 ) return -1;
    if( m_express_status != EXP_OK ) return -1;

    if(m_si570_fitted == true )
    {
        sr_threshold = SR_THRESHOLD_SI570_HZ;
    }
    else
    {
	sr_threshold = SR_S_THRESHOLD_HZ;
    }

    // Maximum default values
	irate = IRATE2;
	clk = sr * 4 * 2;// 4*2 clock rate

    if( sr < (sr_threshold/256) )
	{
		// We can use x64 interpolator
		irate = IRATE64;
		clk = sr * 4 * 64;// 4*64 clock rate
	}
	else
	{
		if( sr < sr_threshold/128 )
		{
			// We can use x32 interpolator
			irate = IRATE32;
			clk = sr * 4 * 32;// 4*32 clock rate
		}
		else
		{
			if( sr < sr_threshold/64 )
			{
				// We can use x16 interpolator
				irate = IRATE16;
				clk = sr * 4 * 16;// 4*16 clock rate
			}
			else
			{
				if( sr < sr_threshold/32 )
				{
					// We can use x8 interpolator
					irate = IRATE8;
					clk = sr * 4 * 8;// 4*8 clock rate
				}
				else
				{
					// We can use x4 interpolator
					if( sr < sr_threshold/16 )
					{
						irate = IRATE4;
						clk = sr * 4 * 4;// 4*4 clock rate
					}
					else
					{
						// We must use the x2 rate interpolator
						irate = IRATE2;
						clk = sr * 4 * 2;// 4*2 clock rate
					}
				}
			}
		}
	}
/*
	if (get_txmode() == M_DVBS2) {
		// Low rate Interpolators not supported for this mode
		if (sr < sr_threshold / 32)
		{
			// We can use x8 interpolator
			irate = IRATE8;
			clk = sr * 4 * 8;// 4*8 clock rate
		}
		else
		{
			// We can use x4 interpolator
			if (sr < sr_threshold / 16)
			{
				irate = IRATE4;
				clk = sr * 4 * 4;// 4*4 clock rate
			}
			else
			{
				// We must use the x2 rate interpolator
				irate = IRATE2;
				clk = sr * 4 * 2;// 4*2 clock rate
			}
		}
	}
*/
    express_set_interp( irate );
    // We need to set the internal SR gen to something
    //sr = sr/400000000.0;//400 MHz;
    // Turn into a 64 bit number
    //val = (long long)(sr*0xFFFFFFFFFFFFFFFF);
	val = (uint64_t)(clk*46116860184.2738790375);
    // Send it
    msg[0]  = FPGA_ADD | I2C_WR;

    msg[1]  = FPGA_SR_REG;
    msg[2]  = (val>>56)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1]  = FPGA_SR_REG+1;
    msg[2]  = (val>>48)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1]  = FPGA_SR_REG+2;
    msg[2]  = (val>>40)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1]  = FPGA_SR_REG+3;
    msg[2]  = (val>>32)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1]  = FPGA_SR_REG+4;
    msg[2]  = (val>>24)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1]  = FPGA_SR_REG+5;
    msg[2]  = (val>>16)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1]  = FPGA_SR_REG+6;
    msg[2]  = (val>>8)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[1]  = FPGA_SR_REG+7;
    msg[2]  = (val)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    express_handle_events( 8 );

    return irate;
}
void express_set_filter( int filter )
{
    unsigned char msg[3];

    if( m_express_status != EXP_OK ) return;

    // Send it
    msg[0] = FPGA_ADD | I2C_WR;
    // MSB
    msg[1] = FPGA_FIL_REG;
    msg[2] = filter;

    express_i2c_bulk_transfer( EP1OUT, msg, 3 );
    express_handle_events( 1 );
}
void express_set_fec( int fec )
{
    unsigned char msg[3];

    if( m_express_status != EXP_OK ) return;

    // Send it
    msg[0] = FPGA_ADD | I2C_WR;
    // MSB
    msg[1] = FPGA_FEC_REG;
    msg[2] = fec;

    express_i2c_bulk_transfer( EP1OUT, msg, 3 );
    express_handle_events( 1 );
}
//
// Send calibration information to the FPGA
//
void express_set_ical( int offset )
{
    short val;
    unsigned char msg[3];

    if( m_express_status != EXP_OK ) return;

    // First do the i channel offset
    val = 0x8000 + offset*4;// bits 1:0 not used
    // Send it
    msg[0]  = FPGA_ADD | I2C_WR;
    // MSB
    msg[1]  = FPGA_I_DC_MSB_REG;
    msg[2]  = (val>>8)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[0]  = FPGA_ADD | I2C_WR;
    // LSB
    msg[1]  = FPGA_I_DC_LSB_REG;
    msg[2]  = val&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    express_handle_events( 2 );

}
void express_set_qcal( int offset )
{
    short val;
    unsigned char msg[3];

    if( m_express_status != EXP_OK ) return;

    // Now the q channel offset
    val = 0x8000 + offset*4;// bits 1:0 not used
    // Send it
    msg[0]  = FPGA_ADD | I2C_WR;
    // MSB
    msg[1]  = FPGA_Q_DC_MSB_REG;
    msg[2]  = (val>>8)&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    msg[0]  = FPGA_ADD | I2C_WR;
    // LSB
    msg[1]  = FPGA_Q_DC_LSB_REG;
    msg[2]  = val&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    express_handle_events( 2 );

}
//
// Send predistortion information to the FPGA
//
void express_load_ptab( uint8_t add, ushort ival, ushort qval )
{
    unsigned char msg[3];

    if( m_express_status != EXP_OK ) return;

    // First send the table address

    msg[0]  = FPGA_ADD | I2C_WR;
    msg[1]  = FPGA_PRE_REG;
    msg[2]  = add;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    // MSB of I value
    msg[0]  = FPGA_ADD | I2C_WR;
    msg[1]  = FPGA_PRE_REG + 1;
    msg[2]  = (ival>>8);
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    // LSB of I value
    msg[0]  = FPGA_ADD | I2C_WR;
    msg[1]  = FPGA_PRE_REG + 2;
    msg[2]  = ival&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    // MSB of Q value
    msg[0]  = FPGA_ADD | I2C_WR;
    msg[1]  = FPGA_PRE_REG + 3;
    msg[2]  = (qval>>8);
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    // LSB of Q value
    msg[0]  = FPGA_ADD | I2C_WR;
    msg[1]  = FPGA_PRE_REG + 4;
    msg[2]  = qval&0xFF;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );


    express_handle_events( 5 );

}

void express_set_config_byte( void )
{
    unsigned char val;
    unsigned char msg[3];

    if( m_express_status != EXP_OK ) return;

    val = 0;
    if(m_tx_hardware == HW_EXPRESS_16 ) val |= FPGA_16BIT_MODE;
    if(m_si570_fitted == true )         val |= FPGA_USE_SI570;
    // Send it
    msg[0]  = FPGA_ADD | I2C_WR;
    msg[1]  = FPGA_CONFIG_REG;
    msg[2]  = val;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );

    express_handle_events( 1 );

}
// End of FPGA I2C messages
void express_set_attenuator_level( char level )
{
    if( m_express_status != EXP_OK ) return;
    // Format up into an I2C message for the ADRF6755
    unsigned char msg[3];
    msg[0]  = ADRF6755_ADD | I2C_WR;//address
    msg[1]  = 30;//CR30
    msg[2]  = 0x00;
    if(level > 0 )
    {
        if(level >= 32 )
        {
           msg[2] |= 0x30;
           level  -= 32;
        }
        if(level >= 16 )
        {
           msg[2] |= 0x10;
           level  -= 16;
        }
        if(level >= 8 )
        {
            msg[2] |= 0x08;
            level  -= 8;
        }
        if(level >= 4 )
        {
            msg[2] |= 0x04;
            level  -= 4;
        }
        if(level >= 2 )
        {
            msg[2] |= 0x02;
            level  -= 2;
        }
        if(level >= 1 )
        {
            msg[2] |= 0x01;
            level  -= 1;
        }
    }
    // Now send it over EP1, should add error checking!
    // The FX2 firmware will send it over I2C
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );
    express_handle_events( 1 );
}
void express_set_level( int level )
{
    if( m_express_status != EXP_OK ) return;
    char a = level&0x7F;

    if( a >= 47 ) a = 47;
    express_set_attenuator_level( a );
}

//
// Read back two samples from the ADC one for each channel
// This needs a seperate callback as it won't complete atomically.
//
void express_read_ad7992_chans( void )
{
    if( m_express_status != EXP_OK ) return;
    unsigned char msg[7];
    // Format up into an I2C message for the ADRF6755
    msg[0]  = AD7992_ADD | I2C_RD;// ADC I2C address
    msg[1]  = 4;//4 bytes response
    msg[2]  = 0x30;// Address data reg and inniate a sample
    // Set address and issue conversion command
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );
    // Read back sample
    express_i2c_bulk_transfer( EP1IN, msg, 7 );

    express_handle_events( 2 );
}
//
// Read back a specific register on the ADC
//
void express_read_ad7992_reg(unsigned char r)
{
    if( m_express_status != EXP_OK ) return;
    // Format up into an I2C message for the AD7992
    unsigned char msg[4];
    msg[0]  = AD7992_ADD | I2C_RD;//address
    msg[1]  = 1;//1 byte response
    msg[2]  = r;// reg to read
    msg[3]  = 0;//Response will go here
    // Set address
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );
    // Read
    express_i2c_bulk_transfer( EP1IN, msg, 4 );

    express_handle_events( 2 );
}
//
// Read and write to the flash memory
//
// Write
// Add is the address to write to
// b is the bytes to be written
// len id the number of bytes to write (max32)
//
void express_write_24c64( int add, unsigned char *b, int len )
{
    if( m_express_status != EXP_OK ) return;
    // Format up into an I2C message for the ADRF6755
    unsigned char msg[40];
    msg[0]  = FLASH_ADD | I2C_WR;//address
    msg[1]  = add>>8;//msb of address
    msg[2]  = add&0xFF;//lsb of address
    // Attach data
    if( len > 32) len = 32;
    for( int i = 0; i < len; i++)
    {
        msg[i+3] = b[i];//data
    }
    // Send
    express_i2c_bulk_transfer( EP1OUT, msg, len + 3 );

    express_handle_events( 1 );
}
// Read
// Add is the address to read from
// b is the bytes to be read
// len id the number of bytes to write (max32 self imposed)
//
void express_read_24c64( int add, int len )
{
    if( m_express_status != EXP_OK ) return;
    // Format up into an I2C message for the ADRF6755
    unsigned char msg[50];
    if(len > 32) len = 32;
    msg[0]  = FLASH_ADD | I2C_RD;//address
    msg[1]  = len;//bytes to receive
    msg[2]  = add>>8;//msb of address
    msg[3]  = add&0xFF;//lsb of address
    // Set read address
    express_i2c_bulk_transfer( EP1OUT, msg, 4 );
    // Read
    express_i2c_bulk_transfer( EP1IN, msg, len + 4 );

    express_handle_events( 2 );
}
//
// Causes the board to break out of it's processing loop
//
void express_breakout(void)
{
    if( m_express_status != EXP_OK ) return;
    // Format up into an I2C message for the ADRF6755
    unsigned char msg[2];
    msg[0]  = FX2_ADD;//address
    msg[1]  = BK_CMD;//Command
    // Set break
    express_i2c_bulk_transfer( EP1OUT, msg, 2 );
    express_handle_events( 1 );
}
//
// Reset the FPGA algorithm
//
void express_fpga_reset(void)
{
    if( m_express_status != EXP_OK ) return;
    // Format up into an I2C message for the ADRF6755
    unsigned char msg[2];
    msg[0]  = FX2_ADD;//address
    msg[1]  = RST_CMD;//Command
    // Set break
    express_i2c_bulk_transfer( EP1OUT, msg, 2 );
    express_handle_events( 1 );
}
//
// Externally callable routines.
//
void express_enable_modulator( void )
{
    if( m_express_status != EXP_OK ) return;
    unsigned char b[3];

    // Power up the modulator, this upsets the attenuator
    b[0]  = ADRF6755_ADD | I2C_WR;//address
    b[1]=29;b[2]=0x01;
    express_i2c_bulk_transfer( EP1OUT, b, 3 );

    express_handle_events( 1 );
}
void express_enable_vco( void )
{
    if( m_express_status != EXP_OK ) return;
    unsigned char b[3];

    // power up internal VCO
    b[1]=28;b[2]=0x01;
    express_i2c_bulk_transfer( EP1OUT, b, 3 );

    express_handle_events( 1 );
}
void express_disable_modulator( void )
{
    if( m_express_status != EXP_OK ) return;
    unsigned char b[3];
    // Power down the modulator, this upsets the attenuator
    b[0]  = ADRF6755_ADD | I2C_WR;//address
    b[1]=29;b[2]=0x00;
    express_i2c_bulk_transfer( EP1OUT, b, 3 );

    express_handle_events( 1 );
}
void express_disable_vco( void )
{
    if( m_express_status != EXP_OK ) return;
    unsigned char b[3];
    b[0]  = ADRF6755_ADD | I2C_WR;//address
    // power down internal VCO
    b[1]=28;b[2]=0x00;
    express_i2c_bulk_transfer( EP1OUT, b, 3 );

    express_handle_events( 1 );
}
void express_transmit(void)
{
    if( m_express_status != EXP_OK ) return;
    unsigned char b[2];
    b[0]  = FX2_ADD | I2C_WR;//address
    // Command
    b[1]=TX_CMD;
    express_i2c_bulk_transfer( EP1OUT, b, 2 );
    express_handle_events( 1 );
}
void express_receive(void)
{
    if( m_express_status != EXP_OK ) return;
    unsigned char b[2];
    b[0]  = FX2_ADD | I2C_WR;//address
    // Command
    b[1]=RX_CMD;
    express_i2c_bulk_transfer( EP1OUT, b, 2 );
    express_handle_events( 1 );
}
void express_ancillary( void )
{
    // Send it
    uint8_t msg[3];
    msg[0]  = FPGA_ADD | I2C_WR;
    msg[1]  = FPGA_ANC_REG;
    msg[2]  = m_ancillary;
    express_i2c_bulk_transfer( EP1OUT, msg, 3 );
    express_handle_events( 1 );
}

void express_set_ports(uint8_t ports) {
	if( m_express_status != EXP_OK ) return;
	m_ancillary = (m_ancillary&0x0F) | (ports<<4);
	express_ancillary();
}


// Set clear carrier output
void express_set_carrier( bool b)
{
    if(b == true )
        m_ancillary = m_ancillary |   FPGA_CARRIER;
    else
        m_ancillary = m_ancillary & (~FPGA_CARRIER);
    express_ancillary();
}

// Set/clear calibration output
void express_set_iqcalibrate( bool b)
{
    if(b == true )
        m_ancillary = m_ancillary |   FPGA_CALIBRA;
    else
        m_ancillary = m_ancillary & (~FPGA_CALIBRA);
    express_ancillary();
}
void express_set_ramp( bool b)
{
    if(b == true )
        m_ancillary = m_ancillary |   FPGA_RAMP;
    else
        m_ancillary = m_ancillary & (~FPGA_RAMP);
    express_ancillary();
}
void express_set_predist( bool b)
{
    if(b == true )
        m_ancillary = m_ancillary |   FPGA_PREDIST;
    else
        m_ancillary = m_ancillary & (~FPGA_PREDIST);
    express_ancillary();
}

//
// Service all pending transfer buffers, used when system is closing
//
void express_release_transfer_buffers(void)
{
    if( m_express_status != EXP_OK ) return;

    struct timeval tm;
    tm.tv_sec =  0;
    tm.tv_usec = 100;

    for( int i = 0; i < N_USB_TX_BUFFS; i++  )
    {
        if(m_xfrs_in_progress > 0) libusb_handle_events_timeout_completed(m_usbctx,&tm,NULL);
    }
}
//
// B = pointer to memory, l size of memory in bytes
//
int express_send_buffer( void *b, int l )
{
    int res = express_bulk_transfer(EP2OUT,(unsigned char *)b, l);
    while( m_xfrs_in_progress > (N_USB_TX_BUFFS/4))
    {
        libusb_handle_events(m_usbctx);
    }
    return res;
}
// Used for testing to insert a single byte into the USB sample stream
void express_insert_bytes(int n)
{
    unsigned char b[255];
    b[0] = 0x00;
    b[1] = 0x01;
    if( n > 255 ) n = 255;
    express_send_buffer( &b, n );
}

//
//
// length is the number of complex samples
// Send each symbol is 2 x 16 bits
//
void express_write_16_bit_samples( scmplx *s, int len )
{
	scmplx *b = (scmplx*)malloc(sizeof(scmplx)*len);
    for( int i = 0; i < len; i++ )
    {
       b[i].re = s[i].re | 0x0001;// Mark I channel, LSB is always '1'
       b[i].im = s[i].im & 0xFFFE;// Mark Q channel, LSB is always '0'
    }
    express_send_buffer( (unsigned char *)b, len*sizeof(scmplx));
}
//
// Send each symbol is 2 x 8 bits
//
int express_write_8_bit_samples( scmplx *s, int len )
{
    unsigned short *b = (unsigned short *)s;
    for( int i = 0; i < len; i++ )
    {
        b[i] = (s[i].re&0xFF00) | ((s[i].im>>8)&0xFF);
    }
    return express_send_buffer((unsigned char*)b, len*sizeof(unsigned short));
}
void express_write_samples( scmplx *s, int len )
{

    if( m_express_status != EXP_OK ) return;
    //
    // Record the sample block size, this should be current
    // but as it is used for metrics does not have to be exact.
    //
    m_sample_block_size = len;

    // Decide which format we are using
    switch( m_tx_hardware )
    {
    case HW_EXPRESS_16:
        express_write_16_bit_samples( s, len );
        break;
    case HW_EXPRESS_8:
        express_write_8_bit_samples( s, len );
        break;
    default:
        express_write_16_bit_samples( s, len );
         break;
    }
}
//
// A Si570 clock generator chip has been detected on the board
//
void express_si570_fitted(void)
{
    m_si570_fitted = true;
    printf("Si570 present\n");
}

//
// Returns the outstanding number of IQ samples left to send in the hardware
// This is an approxiamate number.
//
double express_outstanding_queue_size(void)
{
    double qsize = 0;

    if(m_tx_hardware == HW_EXPRESS_16 )
    {
        qsize = (m_xfrs_in_progress * m_sample_block_size)/2;
        // Add samples in the FPGA FIFO (hardcoded)
        qsize += 2048/2;
    }
    if(m_tx_hardware == HW_EXPRESS_8 )
    {
        qsize = (m_xfrs_in_progress * m_sample_block_size);
        // Add samples in the FPGA FIFO (hardcoded)
        qsize += 2048;
    }
    return qsize;
}
//
// Returns the number of buffers yet to be sent
//
int express_get_tx_queue_size(void)
{
    return m_xfrs_in_progress;
}

void express_deinit(void)
{
    if( m_express_status != EXP_OK ) return;
    express_receive();
    express_wait();
    express_release_transfer_buffers();
    libusb_release_interface(m_handle,0);
    libusb_attach_kernel_driver(m_handle,0);
    libusb_close(m_handle);
    libusb_exit(m_usbctx);
    m_express_status = EXP_CONF;
}

int express_init( const char *fx2_filename, const char *fpga_filename )
{
    char pathname[256];
    m_express_status = EXP_CONF;
    sem_init( &usb_sem, 0, 0 );

    m_xfrs_in_progress = 0;

    // Find the board
    if(( m_express_status = express_find())<0)
    {
        printf("DATV-Express Hardware not found\n");
        return EXP_FAIL;
    }
    // Reset it, this gets it into a state ready for programming
    express_reset();
    express_wait();

    // Load the FX2 firmware
    if(( m_express_status = express_firmware_load( express_firmware_get_path( fx2_filename, pathname )))<0)
    {
        printf("FX2 firmware file not found\n");
        return EXP_IHX;
    }
    express_wait();

    // Start the FX2 code running, this should get the FPGA into a state where it is ready
    // to accept a new program
    express_run();
    express_wait();

    // Load the FPGA firmware
//    if(( m_express_status = express_fpga_load( "/home/charles/BATCExpress.rbf"))<0)
    if(( m_express_status = express_fpga_load( express_firmware_get_path(fpga_filename, pathname )))<0)
    {
        printf("FPGA firmware %s file not found\n",fpga_filename);
        return EXP_RBF;
    }
    express_wait();
    // FPGA firmwre loaded
    m_express_status = EXP_OK;
    //
    // The DATV-Express board should now be in a usuable state both the FX2 and FPGA
    // should be running
    //

    // Send the initial configuration to the Modulator
    express_configure_adrf6755();
    express_wait();
    // Attenuate the ouput
    express_set_attenuator_level( 47 );
    express_wait();
    // Set the operating frequency
    express_set_freq( 1249000000 );
    express_wait();

    // Set symbol rate
    express_set_sr( 1000000 );
    express_wait();

    // Load the configuration byte
    express_set_config_byte();
    express_wait();

    // Zero the Calibration values
    express_set_qcal( 0 );
    express_set_ical( 0 );

    // Configure the ad7992 ADC
    express_configure_ad7992();
    express_wait();

    // Get the ADRF6755 chip version number
    express_read_adrf6755_version();

    // Get the contents of the ad7992 register
    //express_read_ad7992_reg(2);
    express_read_ad7992_chans();

//   unsigned char bb[32];

//   bb[0] = 0xfa;
//   express_write_24c64( 46, bb, 1 );
//   bb[0] = 0;

   express_read_24c64( 32, 16 );

   express_receive();
    return m_express_status;
}
#endif
