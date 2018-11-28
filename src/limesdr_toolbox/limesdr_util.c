/*
  ===========================================================================

  Copyright (C) 2018 Emvivre

  This file is part of LIMESDR_TOOLBOX.

  LIMESDR_TOOLBOX is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  LIMESDR_TOOLBOX is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with LIMESDR_TOOLBOX.  If not, see <http://www.gnu.org/licenses/>.

  ===========================================================================
*/

#include "limesdr_util.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int limesdr_set_channel(const unsigned int freq,
						const double bandwidth_calibrating,
						const double gain,
						const unsigned int channel,
						const char *antenna,
						const int is_tx,
						lms_device_t *device)

{
	bool WithCalibration = true;
	if (access("limemini.cal", F_OK) != -1)
	{
		fprintf(stderr,"Using calibration file\n");
		WithCalibration = false;
	}
	WithCalibration = true;
	int nb_antenna = LMS_GetAntennaList(device, is_tx, channel, NULL);
	lms_name_t list[nb_antenna];
	LMS_GetAntennaList(device, is_tx, channel, list);
	int antenna_found = 0;
	int i;
	for (i = 0; i < nb_antenna; i++)
	{
		if (strcmp(list[i], antenna) == 0)
		{
			antenna_found = 1;
			if (LMS_SetAntenna(device, is_tx, channel, i) < 0)
			{
				fprintf(stderr, "LMS_SetAntenna() : %s\n", LMS_GetLastErrorMessage());
				return -1;
			}
		}
	}
	if (antenna_found == 0)
	{
		fprintf(stderr, "ERROR: unable to found antenna : %s\n", antenna);
		return -1;
	}

	if (WithCalibration)
	{
		//LMS_SetNormalizedGain( device, is_tx, channel, 0 );

		if (LMS_SetLOFrequency(device, is_tx, channel, freq) < 0)
		{
			fprintf(stderr, "LMS_SetLOFrequency() : %s\n", LMS_GetLastErrorMessage());
			return -1;
		}

		if (gain >= 0)
		{
			if (LMS_SetNormalizedGain(device, is_tx, channel, gain) < 0)
			{
				fprintf(stderr, "LMS_SetNormalizedGain() : %s\n", LMS_GetLastErrorMessage());
				return -1;
			}
		}

		if (LMS_Calibrate(device, is_tx, channel, bandwidth_calibrating, 0) < 0)
		{
			fprintf(stderr, "LMS_Calibrate() : %s\n", LMS_GetLastErrorMessage());
			return -1;
		}

		LMS_SetNormalizedGain(device, is_tx, channel, 0);

		if (LMS_SaveConfig(device, "limemini.cal") < 0)
		{
			fprintf(stderr, "LMS_SaveConfig() : %s\n", LMS_GetLastErrorMessage());
			return -1;
		}
	}
	else
	{

		if (LMS_LoadConfig(device, "limemini.cal") < 0)
		{
			fprintf(stderr, "LMS_LoadConfig() : %s\n", LMS_GetLastErrorMessage());
			return -1;
		}
	}
	return 0;
}

int SetGFIR(lms_device_t *device, int Upsample)
{

	//RRC Coeffs with https://www-users.cs.york.ac.uk/~fisher/cgi-bin/mkfscript
	// RRC fc/4
	static double xcoeffs4[] =
		{
			-0.0009155273,
			-0.0001831055,
			+0.0008239746,
			+0.0010681152,
			+0.0002136230,
			-0.0009460449,
			-0.0012207031,
			-0.0002746582,
			+0.0009765625,
			+0.0012512207,
			+0.0001831055,
			-0.0011596680,
			-0.0013427734,
			-0.0000305176,
			+0.0014953613,
			+0.0016479492,
			+0.0000915527,
			-0.0017395020,
			-0.0019226074,
			-0.0001525879,
			+0.0018310547,
			+0.0019531250,
			-0.0000915527,
			-0.0022888184,
			-0.0021667480,
			+0.0004577637,
			+0.0031127930,
			+0.0029296875,
			-0.0003662109,
			-0.0036926270,
			-0.0035705566,
			+0.0002441406,
			+0.0039978027,
			+0.0035705566,
			-0.0011596680,
			-0.0055236816,
			-0.0043029785,
			+0.0025024414,
			+0.0086364746,
			+0.0071105957,
			-0.0021667480,
			-0.0109252930,
			-0.0097351074,
			+0.0018615723,
			+0.0126647949,
			+0.0093078613,
			-0.0086975098,
			-0.0240783691,
			-0.0146179199,
			+0.0230407715,
			+0.0608825684,
			+0.0545043945,
			-0.0183715820,
			-0.1242065430,
			-0.1764831543,
			-0.0840148926,
			+0.1822204590,
			+0.5505371094,
			+0.8722839355,
			+0.9999694824,
			+0.8722839355,
			+0.5505371094,
			+0.1822204590,
			-0.0840148926,
			-0.1764831543,
			-0.1242065430,
			-0.0183715820,
			+0.0545043945,
			+0.0608825684,
			+0.0230407715,
			-0.0146179199,
			-0.0240783691,
			-0.0086975098,
			+0.0093078613,
			+0.0126647949,
			+0.0018615723,
			-0.0097351074,
			-0.0109252930,
			-0.0021667480,
			+0.0071105957,
			+0.0086364746,
			+0.0025024414,
			-0.0043029785,
			-0.0055236816,
			-0.0011596680,
			+0.0035705566,
			+0.0039978027,
			+0.0002441406,
			-0.0035705566,
			-0.0036926270,
			-0.0003662109,
			+0.0029296875,
			+0.0031127930,
			+0.0004577637,
			-0.0021667480,
			-0.0022888184,
			-0.0000915527,
			+0.0019531250,
			+0.0018310547,
			-0.0001525879,
			-0.0019226074,
			-0.0017395020,
			+0.0000915527,
			+0.0016479492,
			+0.0014953613,
			-0.0000305176,
			-0.0013427734,
			-0.0011596680,
			+0.0001831055,
			+0.0012512207,
			+0.0009765625,
			-0.0002746582,
			-0.0012207031,
			-0.0009460449,
			+0.0002136230,
			+0.0010681152,
			+0.0008239746,
			-0.0001831055,
			-0.0009155273,
		};
	double Gain4 = 3.610076904;

	static double xcoeffs2[] =
		{
			-0.0002441406,
			+0.0002441406,
			-0.0000305176,
			-0.0002441406,
			+0.0003051758,
			-0.0000305176,
			-0.0002441406,
			+0.0003356934,
			-0.0000915527,
			-0.0002746582,
			+0.0003967285,
			-0.0001220703,
			-0.0002746582,
			+0.0004577637,
			-0.0001831055,
			-0.0003051758,
			+0.0005187988,
			-0.0002441406,
			-0.0002746582,
			+0.0006103516,
			-0.0003356934,
			-0.0003356934,
			+0.0007019043,
			-0.0003967285,
			-0.0002746582,
			+0.0008239746,
			-0.0005798340,
			-0.0003356934,
			+0.0009765625,
			-0.0007324219,
			-0.0002441406,
			+0.0011901855,
			-0.0010375977,
			-0.0003051758,
			+0.0014038086,
			-0.0012817383,
			-0.0000915527,
			+0.0018615723,
			-0.0019226074,
			-0.0001831055,
			+0.0022277832,
			-0.0025329590,
			+0.0004272461,
			+0.0032958984,
			-0.0040588379,
			+0.0002136230,
			+0.0041198730,
			-0.0061645508,
			+0.0024719238,
			+0.0080566406,
			-0.0120239258,
			+0.0018310547,
			+0.0112915039,
			-0.0267333984,
			+0.0217285156,
			+0.0621337891,
			-0.1262207031,
			-0.1047363281,
			+0.5374145508,
			+0.9999694824,
			+0.5374145508,
			-0.1047363281,
			-0.1262207031,
			+0.0621337891,
			+0.0217285156,
			-0.0267333984,
			+0.0112915039,
			+0.0018310547,
			-0.0120239258,
			+0.0080566406,
			+0.0024719238,
			-0.0061645508,
			+0.0041198730,
			+0.0002136230,
			-0.0040588379,
			+0.0032958984,
			+0.0004272461,
			-0.0025329590,
			+0.0022277832,
			-0.0001831055,
			-0.0019226074,
			+0.0018615723,
			-0.0000915527,
			-0.0012817383,
			+0.0014038086,
			-0.0003051758,
			-0.0010375977,
			+0.0011901855,
			-0.0002441406,
			-0.0007324219,
			+0.0009765625,
			-0.0003356934,
			-0.0005798340,
			+0.0008239746,
			-0.0002746582,
			-0.0003967285,
			+0.0007019043,
			-0.0003356934,
			-0.0003356934,
			+0.0006103516,
			-0.0002746582,
			-0.0002441406,
			+0.0005187988,
			-0.0003051758,
			-0.0001831055,
			+0.0004577637,
			-0.0002746582,
			-0.0001220703,
			+0.0003967285,
			-0.0002746582,
			-0.0000915527,
			+0.0003356934,
			-0.0002441406,
			-0.0000305176,
			+0.0003051758,
			-0.0002441406,
			-0.0000305176,
			+0.0002441406,
			-0.0002441406,
		};
	double Gain2 = 1.743865967;

	for (int i = 0; i < 119; i++)
		xcoeffs4[i] = xcoeffs4[i] / Gain4;
	for (int i = 0; i < 119; i++)
		xcoeffs2[i] = xcoeffs2[i] / Gain2;

	double *xcoeffs = NULL;
	if (Upsample == 2)
		xcoeffs = xcoeffs2;
	if (Upsample == 4)
		xcoeffs = xcoeffs4;
	if (xcoeffs != NULL)
	{
		if (LMS_SetGFIRCoeff(device, LMS_CH_TX, 0, LMS_GFIR3, xcoeffs, 119) < 0)
			fprintf(stderr, "Unable to set coeff GFIR3");
		return (LMS_SetGFIR(device, LMS_CH_TX, 0, LMS_GFIR3, true));
	}
	else
		return 0;
}

int limesdr_init(const double sample_rate,
				 const unsigned int freq,
				 const double bandwidth_calibrating,
				 const double gain,
				 const unsigned int device_i,
				 const unsigned int channel,
				 const char *antenna,
				 const int is_tx,
				 lms_device_t **device,
				 double *host_sample_rate)
{
	int device_count = LMS_GetDeviceList(NULL);
	if (device_count < 0)
	{
		fprintf(stderr, "LMS_GetDeviceList() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	lms_info_str_t device_list[device_count];
	if (LMS_GetDeviceList(device_list) < 0)
	{
		fprintf(stderr, "LMS_GetDeviceList() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	if (LMS_Open(device, device_list[device_i], NULL) < 0)
	{
		fprintf(stderr, "LMS_Open() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	if (LMS_Reset(*device) < 0)
	{
		fprintf(stderr, "LMS_Reset() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	if (LMS_Init(*device) < 0)
	{
		fprintf(stderr, "LMS_Init() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	int is_not_tx = (is_tx == LMS_CH_TX) ? LMS_CH_RX : LMS_CH_TX;
	if (LMS_EnableChannel(*device, is_not_tx, channel, false) < 0)
	{
		fprintf(stderr, "LMS_EnableChannel() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	if (LMS_EnableChannel(*device, is_not_tx, 1 - channel, false) < 0)
	{
		fprintf(stderr, "LMS_EnableChannel() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	if (LMS_EnableChannel(*device, is_tx, 1 - channel, false) < 0)
	{
		fprintf(stderr, "LMS_EnableChannel() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	if (LMS_EnableChannel(*device, is_tx, channel, true) < 0)
	{
		fprintf(stderr, "LMS_EnableChannel() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	if (LMS_SetSampleRate(*device, sample_rate, 0) < 0)
	{
		fprintf(stderr, "LMS_SetSampleRate() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}
	if (LMS_GetSampleRate(*device, is_tx, channel, host_sample_rate, NULL) < 0)
	{
		fprintf(stderr, "LMS_GetSampleRate() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}

	if (limesdr_set_channel(freq, bandwidth_calibrating, gain, channel, antenna, is_tx, *device) < 0)
	{
		return -1;
	}

	return 0;
}
