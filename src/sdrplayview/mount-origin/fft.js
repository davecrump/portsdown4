'use strict';


function Spectrum(spectrumCanvasId, colourMap)
{
    this.width = document.getElementById(spectrumCanvasId).clientWidth;
    this.height = document.getElementById(spectrumCanvasId).clientHeight;

    this.ctx = document.getElementById(spectrumCanvasId).getContext("2d");

    this.map = colourMap;

    this.updateData = function(data)
    {
      var dataLength = 300;//data.length;
      var i;
      var sample_index;
      var sample_index_f;
      var sample;
      var sample_fraction;

      this.ctx.clearRect(0, 0, this.width, this.height);

      for(i=0; i<this.width; i++)
        {
          sample_index = (i*dataLength)/ this.width;
          sample_index_f = sample_index | 0;
          sample = data[sample_index_f]
             + (sample_index - sample_index_f) * (data[sample_index_f+1] - data[sample_index_f]);
          sample_fraction = sample / 65535;
          sample = (sample * (256.0 / 65536)) | 0;
          this.ctx.fillStyle = "rgba("+this.map[sample][0]+","+this.map[sample][1]+","+this.map[sample][2]+",255)";
          this.ctx.fillRect(i, this.height-(sample_fraction * this.height), 2, 2);
        }
    };
}


function reverse_bits(val, width) {
	var result = 0;
	for (let i = 0; i < width; i++, val >>= 1)
		result = (result << 1) | (val & 1);
	return result;
}

//var fft_dat = new Float32Array(2048);
var fft_dat = new Float32Array(2049);

function Fft() {
//function Fft_transformRadix2(double real[restrict], double imag[restrict], size_t n) {
//	levels = 0;  // Compute levels = floor(log2(n))
//	for (temp = n; temp > 1; temp >>= 1)
//		levels++;
//	if (1 << levels != n)
//		return false;  // n is not a power of 2
	var n=1024;
        var levels=10
	// Trigonometric tables
//	if (SIZE_MAX / sizeof(double) < n / 2)
//		return false;
//	size_t size = (n / 2) * sizeof(double);
//	double *cos_table = malloc(size);
//	double *sin_table = malloc(size);
//	for (size_t i = 0; i < n / 2; i++) {
//		cos_table[i] = cos(2 * M_PI * i / n);
//		sin_table[i] = sin(2 * M_PI * i / n);
//	}
	
	// Bit-reversed addressing permutation
	for (let i = 0; i < n; i++) {
//	for (size_t i = 0; i < n; i++) {
		var j = reverse_bits(i, levels);
//		size_t j = reverse_bits(i, levels);
		if (j > i) {
			var temp = fft_dat[i*2]; //real[i];
			fft_dat[i*2] = fft_dat[j*2]; //real[i] = real[j];
			fft_dat[j*2] = temp; //real[j] = temp;
			temp = fft_dat[i*2+1]; //imag[i];
			fft_dat[i*2+1] = fft_dat[j*2+1]; //imag[i] = imag[j];
			fft_dat[j*2+1] = temp; //imag[j] = temp;
		}
	}
	
	// Cooley-Tukey decimation-in-time radix-2 FFT
	for (let size = 2; size <= n; size *= 2) {
		var halfsize = size / 2;
		var tablestep = n / size;
		for (let i = 0; i < n; i += size) {
			for (let j = i, k = 0; j < i + halfsize; j++, k += tablestep) {
				var l = j + halfsize;
				var cos_table = Math.cos(2 * Math.PI * k / n);
				var sin_table = Math.sin(2 * Math.PI * k / n);
				var tpre =  fft_dat[l*2] * cos_table + fft_dat[l*2+1] * sin_table;
				var tpim = -fft_dat[l*2] * sin_table + fft_dat[l*2+1] * cos_table;
//				double tpre =  real[l] * cos_table + imag[l] * sin_table;
//				double tpim = -real[l] * sin_table + imag[l] * cos_table;
				fft_dat[l*2] = fft_dat[j*2] - tpre; //real[l] = real[j] - tpre;
				fft_dat[l*2+1] = fft_dat[j*2+1] - tpim; //imag[l] = imag[j] - tpim;
				fft_dat[j*2] += tpre; //real[j] += tpre;
				fft_dat[j*2+1] += tpim; //imag[j] += tpim;
			}
		}
		if (size == n)  // Prevent overflow in 'size *= 2'
			break;
	}
}



/* Andy JNT's basic version translated */

function fft() {
    var nn = 2 * 1024;
    var j = 1;
    var m;
    var mmax = 2;
    var istep;
    var theta;
    var wpi, wpr, wr, wi;
    var swap;
    var tempr, tempi;
    var wtemp;
    
    for (let i = 1; i<=nn; i+=2) { // Bit reversed addressing first 
        if (j > i) {
            swap     = fft_dat[j];
            fft_dat[j]   = fft_dat[i];
            fft_dat[i]   = swap;
            swap     = fft_dat[j+1];
            fft_dat[j+1] = fft_dat[i+1];
            fft_dat[i+1] = swap;
        }
        m = nn / 2;
        while ((m>=2) && (j>m)) {
            j = j - m;
            m = m / 2;
        }
        j = j + m;
    }

    while (nn > mmax) {
        istep = 2 * mmax;
        theta = 2 * Math.PI / mmax;
        wpr = Math.cos(theta);
        wpi = Math.sin(theta);
        wr = 1;
        wi = 0;
        for (m=1; m<=mmax; m+=2) {
            for (let i=m; i<=nn; i+=istep) {
                j = i + mmax;
                tempr = wr * fft_dat[j  ] - wi * fft_dat[j+1];
                tempi = wr * fft_dat[j+1] + wi * fft_dat[j  ];
                fft_dat[j  ] = fft_dat[i  ] - tempr;
                fft_dat[j+1] = fft_dat[i+1] - tempi;
                fft_dat[i  ] = fft_dat[i  ] + tempr;
                fft_dat[i+1] = fft_dat[i+1] + tempi;
            }
            wtemp = wr;
            wr = wr * wpr - wi    * wpi;
            wi = wi * wpr + wtemp * wpi;
        }
        mmax = istep;
    }
 //   for (let i=0;i<200;i++) {
//        console.log(i,fft_dat[i*2]);
//    }
}

function HammingWind(dat) {
    var theta;
    var i_dat, q_dat;
    
    for (let k = 0; k < 1024; k++) {
        theta = 2.0 * Math.PI * k / 1024;
        i_dat = dat[k*4]+256*dat[k*4+1];
        if (i_dat > 32767) i_dat -= 65536;
        q_dat = dat[k*4+2]+256*dat[k*4+3];
        if (q_dat > 32767) q_dat -= 65536;
//        i_dat = Math.sin(k/1024*2*Math.PI*450);
//        q_dat = 0.0;
        i_dat = i_dat/32768; //32768*Math.sin(k/1024*2*Math.PI*495);
        q_dat = q_dat/32768; //0.0;

        fft_dat[k*2+1] = i_dat * (0.54 - 0.46 * Math.cos(theta));
        fft_dat[k*2+2] = q_dat * (0.54 - 0.46 * Math.cos(theta));
    }
}


function Waterfall(canvasFrontId, colourMap)
{
    this.width = document.getElementById(canvasFrontId).clientWidth;
    this.height = document.getElementById(canvasFrontId).clientHeight;
    this.map = colourMap;

    this.imgFront = document.getElementById(canvasFrontId);
    this.ctxFront = this.imgFront.getContext("2d");

    this.lineImage = this.ctxFront.createImageData(this.width, 1);

    this.addLine = function(data)
    {
      var dataLength = 1024; //data.length/2;
      var imgdata = this.lineImage.data;
      var lookup = 0;
      var i = 0;
      let sample;
      let sample_fraction;
      let sample_index;
      let sample_index_f;
      var WFGain   = parseFloat(document.getElementById("WFGain").value);
      var WFOffset = parseFloat(document.getElementById("WFOffset").value);

      // when we get here we have 1024 points of amplitude i,q interleaved samples
      // we first need to do an fft on these to get them into a spectrum plot
      HammingWind(data);
      fft();
//      console.log("After FFT ",fft_dat[1],fft_dat[2],fft_dat[3]);
//      Fft();
//      for (lookup = 0; lookup < this.width; lookup++)
      for (lookup = 0; lookup < 1024; lookup++)
      {
//        sample_index = (lookup*dataLength)/this.width;
//        sample_index_f = sample_index | 0;
//        sample = data[sample_index_f]
//           + (sample_index - sample_index_f) * (data[sample_index_f+1] - data[sample_index_f]);
//        sample = fft_dat[lookup * 8] + 256 * data[lookup * 8 + 1];
//        if (sample > 32767) sample -= 65536;
//        sample += 32768; 
//        sample_fraction = sample * (256 / 65536);

        // note our FFT produced data in dat[1] to dat[1024] (BASIC is a terrible thing)
        // we want to have min 0 and max 255 for our display output  
        // also, the lower half is the positive frquencies and the upper is the negative
        // so the array needs re-arranging to get a sensible waterfall
        var index;
        if (lookup<512) index = (lookup+512)*2+1;
        else index = 2*(lookup-512)+1;
        var re=fft_dat[index  ];
        var im=fft_dat[index+1];
        sample_fraction = Math.sqrt(re*re+im*im);
        sample_fraction = sample_fraction * WFGain;
        sample_fraction = sample_fraction + WFOffset;
        if (sample_fraction < 0  ) sample_fraction = 0;
        if (sample_fraction > 255) sample_fraction = 255;
        var rgb = this.map[sample_fraction | 0];
        imgdata[i++] = rgb[0];
        imgdata[i++] = rgb[1];
        imgdata[i++] = rgb[2];
        imgdata[i++] = 255;
      }
      this.ctxFront.drawImage(this.imgFront, 
                    0, 0, this.width, this.height - 1, 
                    0, 1, this.width, this.height - 1);
      this.ctxFront.putImageData(this.lineImage, 0, 0);

      if (this.existingHeight < (this.height-1))
      {
        this.existingHeight++;
      }
    };
}

function ColourMap()
{
  var map = new Array(256);

  var e;
  for (e = 0; 64 > e; e++)
  {
    map[e] = new Uint8Array(3);
    map[e][0] = 0;
    map[e][1] = 0;
    map[e][2] = 64 + e;
  }
  for (; 128 > e; e++)
  {
    map[e] = new Uint8Array(3);
    map[e][0] = 3 * e - 192;
    map[e][1] = 0;
    map[e][2] = 64 + e;
  }
  for (; 192 > e; e++)
  {
    map[e] = new Uint8Array(3);
    map[e][0] = e + 64;
    map[e][1] = 256 * Math.sqrt((e - 128) / 64);
    map[e][2] = 511 - 2 * e;
  }
  for (; 256 > e; e++)
  {
    map[e] = new Uint8Array(3);
    map[e][0] = 255;
    map[e][1] = 255;
    map[e][2] = 512 + 2 * e;
  }

  return map;
}
