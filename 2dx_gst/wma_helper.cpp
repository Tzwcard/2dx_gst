/*
** REF: https://docs.microsoft.com/en-us/windows/desktop/medfound/tutorial--decoding-audio
*/

#include "wma_helper.h"

#include <Shlwapi.h>
#include <thread>
#include <mutex>          // std::mutex
#include <fstream>

#include "outdef.h"

#pragma comment(lib, "Shlwapi.lib")

static wchar_t tmp_prefix[4] = L"wma",
tmp_file_path[MAX_PATH],
tmp_file_path_filename[MAX_PATH];

template <class T> void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

//-------------------------------------------------------------------
// ConfigureAudioStream
//
// Selects an audio stream from the source file, and configures the
// stream to deliver decoded PCM audio.
//-------------------------------------------------------------------

static HRESULT ConfigureAudioStream(
    IMFSourceReader *pReader,   // Pointer to the source reader.
    IMFMediaType **ppPCMAudio   // Receives the audio format.
    )
{
    IMFMediaType *pUncompressedAudioType = NULL;
    IMFMediaType *pPartialType = NULL;

    // Select the first audio stream, and deselect all other streams.
    HRESULT hr = pReader->SetStreamSelection(
        (DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);

    if (SUCCEEDED(hr))
    {
        hr = pReader->SetStreamSelection(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    }

    // Create a partial media type that specifies uncompressed PCM audio.
    hr = MFCreateMediaType(&pPartialType);

    if (SUCCEEDED(hr))
    {
        hr = pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    }

    if (SUCCEEDED(hr))
    {
        hr = pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    }

	pPartialType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, __OUTPUT_WAVEFORM_CHANNELS);
	pPartialType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, __OUTPUT_WAVEFORM_SAMPLE_PER_SECOND);
	pPartialType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, sizeof(__OUTPUT_WAVEFORM_TYPE)* 8);
	pPartialType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, __OUTPUT_WAVEFORM_SAMPLE_PER_SECOND * __OUTPUT_WAVEFORM_CHANNELS * sizeof(__OUTPUT_WAVEFORM_TYPE));
	pPartialType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, __OUTPUT_WAVEFORM_CHANNELS * sizeof(__OUTPUT_WAVEFORM_TYPE));

    // Set this type on the source reader. The source reader will
    // load the necessary decoder.
    if (SUCCEEDED(hr))
    {
        hr = pReader->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            NULL, pPartialType);
    }

    // Get the complete uncompressed format.
    if (SUCCEEDED(hr))
    {
        hr = pReader->GetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            &pUncompressedAudioType);
    }

    // Ensure the stream is selected.
    if (SUCCEEDED(hr))
    {
        hr = pReader->SetStreamSelection(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            TRUE);
    }

    // Return the PCM format to the caller.
    if (SUCCEEDED(hr))
    {
        *ppPCMAudio = pUncompressedAudioType;
        (*ppPCMAudio)->AddRef();
    }

    SafeRelease(&pUncompressedAudioType);
    SafeRelease(&pPartialType);
    return hr;
}

//-------------------------------------------------------------------
// WriteWaveData
//
// Decodes PCM audio data from the source file and writes it to
// the WAVE file.
//-------------------------------------------------------------------

static HRESULT WriteWaveData(
    unsigned char *data_ptr,               // Output file.
    IMFSourceReader *pReader,   // Source reader.
    DWORD *pcbDataWritten       // Receives the amount of data written.
    )
{
    HRESULT hr = S_OK;
    DWORD cbAudioData = 0;
    DWORD cbBuffer = 0;
    BYTE *pAudioData = NULL;

    IMFSample *pSample = NULL;
    IMFMediaBuffer *pBuffer = NULL;

    // Get audio samples from the source reader.
    while (true)
    {
        DWORD dwFlags = 0;

        // Read the next sample.
        hr = pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0, NULL, &dwFlags, NULL, &pSample );

        if (FAILED(hr)) { break; }

        if (dwFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
        {
            printf("Type change - not supported by WAVE file format.\n");
            break;
        }
        if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
//            printf("End of input file.\n");
            break;
        }

        if (pSample == NULL)
        {
            printf("No sample\n");
            continue;
        }

        // Get a pointer to the audio data in the sample.

        hr = pSample->ConvertToContiguousBuffer(&pBuffer);

        if (FAILED(hr)) { break; }


        hr = pBuffer->Lock(&pAudioData, NULL, &cbBuffer);

        if (FAILED(hr)) { break; }

#if 0
        // Make sure not to exceed the specified maximum size.
        if (cbMaxAudioData - cbAudioData < cbBuffer)
        {
            cbBuffer = cbMaxAudioData - cbAudioData;
		}
#endif

        // Write this data to the output file.
//        hr = WriteToFile(hFile, pAudioData, cbBuffer);
		memcpy(data_ptr + cbAudioData, pAudioData, cbBuffer);

        if (FAILED(hr)) { break; }

        // Unlock the buffer.
        hr = pBuffer->Unlock();
        pAudioData = NULL;

        if (FAILED(hr)) { break; }

        // Update running total of audio data.
        cbAudioData += cbBuffer;

#if 0
        if (cbAudioData >= cbMaxAudioData)
        {
            break;
        }
#endif

        SafeRelease(&pSample);
        SafeRelease(&pBuffer);
    }

    if (SUCCEEDED(hr))
    {
  //      printf("Wrote %d bytes of audio data.\n", cbAudioData);

		*pcbDataWritten = cbAudioData;
    }

    if (pAudioData)
    {
        pBuffer->Unlock();
    }

    SafeRelease(&pBuffer);
    SafeRelease(&pSample);
    return hr;
}

static int WriteWave(
	IMFSourceReader *pReader, 
	unsigned char *wave_data,
	DWORD *wave_size
	)
{
	HRESULT hr = S_OK;

    IMFMediaType *pAudioType = NULL;    // Represents the PCM audio format.

    // Configure the source reader to get uncompressed PCM audio from the source file.

    hr = ConfigureAudioStream(pReader, &pAudioType);

    // Calculate the maximum amount of audio to decode, in bytes.
    if (SUCCEEDED(hr))
    {
//        *wave_size = CalculateMaxAudioDataSize(pAudioType, cbHeader, msecAudioData);

        // Decode audio data to the file.
        hr = WriteWaveData(wave_data, pReader, wave_size);
    }

    SafeRelease(&pAudioType);
    return hr;
}

int wma_to_waveform(unsigned char *wma_data, int size_wma_data, unsigned char *wave_data)
{
//	printf("wma_to_waveform(%p, %d, %p) called.\n", wma_data, size_wma_data, wave_data);

	DWORD wave_size = 0;

    HRESULT hr = S_OK;

    IMFSourceReader *pReader = NULL;
//	myIMFByteStream byteStream;

//	byteStream.set_data(wma_data, size_wma_data);

	/*
	** TODO: Memory reading, no more temporary files
	*/
    if (SUCCEEDED(hr))
    {
#if 0
		hr = MFCreateSourceReaderFromByteStream(&byteStream, NULL, &pReader);
        if (FAILED(hr))
        {
			printf("Error reading from stream: %p\n", &byteStream);
        }
#else
		HANDLE file = CreateFileW(tmp_file_path_filename,
			GENERIC_READ | GENERIC_WRITE, 
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, 
			CREATE_ALWAYS, 
			FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
			NULL
			);
		DWORD written;
		WriteFile(file, wma_data, size_wma_data, &written, NULL);

		hr = MFCreateSourceReaderFromURL(tmp_file_path_filename, NULL, &pReader);
		if (FAILED(hr))
		{
			printf("Error opening input file: %S\n", tmp_file_path_filename, hr);
		}

		CloseHandle(file);
#endif
    }
//	printf("MFSourceReader created.\n");

    // Write the WAVE file.
    if (SUCCEEDED(hr))
    {
        hr = WriteWave(pReader, wave_data, &wave_size);
    }

    if (FAILED(hr))
    {
        printf("Failed, hr = 0x%X\n", hr);
    }

    SafeRelease(&pReader);

	return wave_size;
}

int wma_helper_initialize(void)
{
	// Initialize the COM library.
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	// Initialize the Media Foundation platform.
	if (SUCCEEDED(hr))
	{
		hr = MFStartup(MF_VERSION);
	}

	int ret = SUCCEEDED(hr) ? 1 : 0;

	GetTempPathW(MAX_PATH, tmp_file_path);
	GetTempFileNameW(tmp_file_path, tmp_prefix, 0, tmp_file_path_filename);

//	printf("Temp file as '%S'\n", tmp_file_path_filename);
	return ret;
}

void wma_helper_uninitialize(void)
{
	MFShutdown();
	CoUninitialize();
	if (PathFileExistsW(tmp_file_path_filename))
		DeleteFileW(tmp_file_path_filename);
}