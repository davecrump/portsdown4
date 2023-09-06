//      const playButton = document.querySelector("button");
      let f;
      // Create AudioContext and buffer source
      let audioCtx;
      let source;
      // create the buffer. this is an array of 10 Float32Arrays. filled with 0.0 to start
      let audioBufferL = new Array(10);
      let audioBufferR = new Array(10);
      for (var i = 0; i < 10; i++) {
          audioBufferL[i] = new Float32Array(4096); // make each element an array
          audioBufferR[i] = new Float32Array(4096); // make each element an array
      }
      let in_pointer = 0;
      let out_pointer = 0;
      let buffer_chunk = 0;

      // each input byte buffer of 4096 bytes contains 1024 S16 stereo samples (IQ sample)
      //  I[0] = byte[0] + 256 * byte[1]
      //  Q[0] = byte[2] + 256 * byte[3]    and so on.
      // we need to extract this into teo audio output buffers and convert to float32 as we go
      // each output buffer is 4096 floats. we have 2 buffers, one for left and one for right (I and Q)
      function add_into_buffer(byte_buffer) {
          var AudioGain = parseFloat(document.getElementById("AudioGain").value);
          for (
              let posn = 0;
              posn < 4096;
              posn += 4
          ) {
              if ((posn==0) && (buffer_chunk==0)) {
                  audioBufferL[in_pointer] = new Float32Array(4096); // make each element an array
                  audioBufferR[in_pointer] = new Float32Array(4096); // make each element an array
              }
              L = byte_buffer[posn    ] + 256 * byte_buffer[posn + 1]; 
              R = byte_buffer[posn + 2] + 256 * byte_buffer[posn + 3];
              if (L > 32767) L -= 65536; 
              if (R > 32767) R -= 65536; 
              audioBufferL[in_pointer][posn/4 + 1024 * buffer_chunk] = L / 32768 * AudioGain; 
              audioBufferR[in_pointer][posn/4 + 1024 * buffer_chunk] = R / 32768 * AudioGain; 
          }
          // move on to the next chunk and wrap if needed
          buffer_chunk++;
          if (buffer_chunk == 4) {
              buffer_chunk = 0;
              // if we have filled all of this buffer entry, then move the pointer on one (or wrap)
//              console.log("full Buffer", in_pointer,
//                      audioBufferL[in_pointer][   0]*32768, audioBufferR[in_pointer][   0]*32768,
//                      audioBufferL[in_pointer][   1]*32768, audioBufferR[in_pointer][   1]*32768,
//                      audioBufferL[in_pointer][1022]*32768, audioBufferR[in_pointer][1022]*32768,
//                      audioBufferL[in_pointer][1023]*32768, audioBufferR[in_pointer][1023]*32768,
//                      audioBufferL[in_pointer][1024]*32768, audioBufferR[in_pointer][1024]*32768,
//                      audioBufferL[in_pointer][1025]*32768, audioBufferR[in_pointer][1025]*32768,
//                      audioBufferL[in_pointer][2046]*32768, audioBufferR[in_pointer][2046]*32768,
//                      audioBufferL[in_pointer][2047]*32768, audioBufferR[in_pointer][2047]*32768,
//                      audioBufferL[in_pointer][2048]*32768, audioBufferR[in_pointer][2048]*32768,
//                      audioBufferL[in_pointer][2049]*32768, audioBufferR[in_pointer][2049]*32768,
//                      audioBufferL[in_pointer][3070]*32768, audioBufferR[in_pointer][3070]*32768,
//                      audioBufferL[in_pointer][3071]*32768, audioBufferR[in_pointer][3071]*32768,
//                      audioBufferL[in_pointer][3072]*32768, audioBufferR[in_pointer][3072]*32768,
//                      audioBufferL[in_pointer][3073]*32768, audioBufferR[in_pointer][3073]*32768,
//                      audioBufferL[in_pointer][4094]*32768, audioBufferR[in_pointer][4094]*32768,
//                      audioBufferL[in_pointer][4095]*32768, audioBufferR[in_pointer][4095]*32768);
              in_pointer++;
              if (in_pointer == 10) {
                  in_pointer = 0;
              }
          }
      }

      function init() {
          audioCtx = new AudioContext({sampleRate: 10000});
          source = audioCtx.createBufferSource();
          f = 0.0;
          // Create a ScriptProcessorNode with a bufferSize of 4096 no input and two output channel
          let scriptNode = audioCtx.createScriptProcessor(4096, 0, 2);
          console.log("Init", scriptNode.bufferSize);
  
          // we start with the output pointer half way through the buffer (i.e add 5)) we may need to wrap it
          out_pointer = in_pointer + 5;
          if (out_pointer > 9) {
              out_pointer -= 10;
          }

          // Give the node a function to process audio events
          // this is called each time new audio data is required
          // we handle this by filling the audio buffer from data we have stored in our circular buffer 
          scriptNode.onaudioprocess = function (audioProcessingEvent) {
              console.log("Audio Process", scriptNode.bufferSize, in_pointer, out_pointer);

              // The output buffer contains the samples that will be modified and played
              let outputBuffer = audioProcessingEvent.outputBuffer;

              // Loop through the output channels (in this case there is only one)
              let outputDataR = outputBuffer.getChannelData(0);
              let outputDataL = outputBuffer.getChannelData(1);

              // Loop through the 4096 samples copying the data from the audio buffer into the outputBuffer
              for (let sample = 0; sample < outputBuffer.length; sample++) {
//                if (channel==1) {
//    	              outputData[sample] = Math.sin(f)* 0.1;
//	              f+=0.25;
//	              if (f>2*Math.PI) {f-=2*Math.PI;}
//	          } else {
//	              outputData[sample] = (Math.random() * 2 - 1) * 0.1;
//	          }
                  outputDataL[sample] = audioBufferL[out_pointer][sample];
                  outputDataR[sample] = audioBufferR[out_pointer][sample];
              }
          
              // move the output buffer on one once we have extracted the data (and wrap if needed)
              out_pointer++;
              if (out_pointer == 10) {
                  out_pointer = 0;
              }
          };

          source.connect(scriptNode);
          scriptNode.connect(audioCtx.destination);
          source.start();

          // When the buffer source stops playing, disconnect everything
          source.onended = function () {
              source.disconnect(scriptNode);
              scriptNode.disconnect(audioCtx.destination);
          };
      }

      document.addEventListener("DOMContentLoaded", function() {
          var ele = document.getElementById("playButton");
          ele.addEventListener("click", init);
      });

