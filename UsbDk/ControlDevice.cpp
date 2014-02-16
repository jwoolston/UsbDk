#include "ControlDevice.h"
#include "trace.h"
#include "ControlDevice.tmh"
#include "Public.h"

class CUsbDkControlDeviceInit : public CDeviceInit
{
public:
    CUsbDkControlDeviceInit()
    {}

    NTSTATUS Create(WDFDRIVER Driver);

    CUsbDkControlDeviceInit(const CUsbDkControlDeviceInit&) = delete;
    CUsbDkControlDeviceInit& operator= (const CUsbDkControlDeviceInit&) = delete;
};

NTSTATUS CUsbDkControlDeviceInit::Create(WDFDRIVER Driver)
{
    if (!CDeviceInit::Create(Driver, SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX))
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    SetExclusive();
    SetIoBuffered();

    DECLARE_CONST_UNICODE_STRING(ntDeviceName, USBDK_DEVICE_NAME);
    return SetName(ntDeviceName);
}

void CUsbDkControlDeviceQueue::SetCallbacks(WDF_IO_QUEUE_CONFIG &QueueConfig)
{
    QueueConfig.EvtIoDeviceControl = CUsbDkControlDeviceQueue::DeviceControl;
}

void CUsbDkControlDeviceQueue::DeviceControl(WDFQUEUE Queue,
                                             WDFREQUEST Request,
                                             size_t OutputBufferLength,
                                             size_t InputBufferLength,
                                             ULONG IoControlCode)
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CONTROLDEVICE, "%!FUNC! Request arrived");

    switch (IoControlCode)
    {
        case IOCTL_USBDK_PING:
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_CONTROLDEVICE, "Called IOCTL_USBDK_PING\n");

            LPTSTR outBuff;
            size_t outBuffLen;
            status = WdfRequestRetrieveOutputBuffer(Request, 0, (PVOID *)&outBuff, &outBuffLen);
            if (!NT_SUCCESS(status)) {
                break;
            }

            wcsncpy(outBuff, TEXT("Pong!"), outBuffLen/sizeof(TCHAR));
            WdfRequestSetInformation(Request, outBuffLen);
            status = STATUS_SUCCESS;
        }
        break;
    }

    WdfRequestComplete(Request, status);
}

NTSTATUS CUsbDkControlDevice::Create(WDFDRIVER Driver)
{
    CUsbDkControlDeviceInit DeviceInit;

    auto status = DeviceInit.Create(Driver);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    WDF_OBJECT_ATTRIBUTES attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, USBDK_CONTROL_DEVICE_EXTENSION);
    status = CWdfControlDevice::Create(DeviceInit, attr);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    DECLARE_CONST_UNICODE_STRING(ntDosDeviceName, USBDK_DOSDEVICE_NAME);
    status = CreateSymLink(ntDosDeviceName);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    m_DeviceQueue = new CUsbDkControlDeviceQueue(*this, WdfIoQueueDispatchSequential);
    status = m_DeviceQueue->Create();
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    FinishInitializing();

    return STATUS_SUCCESS;
}