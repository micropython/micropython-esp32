/*
 * This file is part of the MicroPython project, http://micropython.org/
 */

/**
  ******************************************************************************
  * @file    USB_Device/CDC_Standalone/Src/usbd_conf.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    26-February-2014
  * @brief   This file implements the USB Device library callbacks and MSP
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_core.h"
#include "py/obj.h"
#include "irq.h"
#include "usb.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
#ifdef USE_USB_FS
PCD_HandleTypeDef pcd_fs_handle;
#endif
#ifdef USE_USB_HS
PCD_HandleTypeDef pcd_hs_handle;
#endif
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
                       PCD BSP Routines
*******************************************************************************/
/**
  * @brief  Initializes the PCD MSP.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
  GPIO_InitTypeDef  GPIO_InitStruct;
  
  if(hpcd->Instance == USB_OTG_FS)
  {
    /* Configure USB FS GPIOs */
    __GPIOA_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
    
	/* Configure VBUS Pin */
#if defined(MICROPY_HW_USB_VBUS_DETECT_PIN)
    // USB VBUS detect pin is always A9
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
#endif
	
#if defined(MICROPY_HW_USB_OTG_ID_PIN)
    // USB ID pin is always A10
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
#endif

    /* Enable USB FS Clocks */ 
    __USB_OTG_FS_CLK_ENABLE();
    
#if defined (MCU_SERIES_L4)
    /* Enable VDDUSB */
    if(__HAL_RCC_PWR_IS_CLK_DISABLED())
    {
      __HAL_RCC_PWR_CLK_ENABLE();
      HAL_PWREx_EnableVddUSB();
      __HAL_RCC_PWR_CLK_DISABLE();
    }
    else
    {
      HAL_PWREx_EnableVddUSB();
    }
#endif

    /* Set USBFS Interrupt priority */
    HAL_NVIC_SetPriority(OTG_FS_IRQn, IRQ_PRI_OTG_FS, IRQ_SUBPRI_OTG_FS);
    
    /* Enable USBFS Interrupt */
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
  } 
#if defined(USE_USB_HS)
  else if(hpcd->Instance == USB_OTG_HS)
  {
#if defined(USE_USB_HS_IN_FS)

    /* Configure USB FS GPIOs */
    __GPIOB_CLK_ENABLE();

    /* Configure DM DP Pins */
    GPIO_InitStruct.Pin = (GPIO_PIN_14 | GPIO_PIN_15);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

#if defined(MICROPY_HW_USB_VBUS_DETECT_PIN)
    /* Configure VBUS Pin */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif

#if defined(MICROPY_HW_USB_OTG_ID_PIN)
    /* Configure ID pin */
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
#endif
    /*
     * Enable calling WFI and correct
     * function of the embedded USB_FS_IN_HS phy
     */
    __OTGHSULPI_CLK_SLEEP_DISABLE();
    __OTGHS_CLK_SLEEP_ENABLE();
    /* Enable USB HS Clocks */
    __USB_OTG_HS_CLK_ENABLE();

#else // !USE_USB_HS_IN_FS

    /* Configure USB HS GPIOs */
    __GPIOA_CLK_ENABLE();
    __GPIOB_CLK_ENABLE();
    __GPIOC_CLK_ENABLE();
    __GPIOH_CLK_ENABLE();
    __GPIOI_CLK_ENABLE();
    
    /* CLK */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
    
    /* D0 */
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
    
    /* D1 D2 D3 D4 D5 D6 D7 */
    GPIO_InitStruct.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_5 |\
                          GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); 
    
    /* STP */    
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct); 
    
    /* NXT */ 
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);   
    
    /* DIR */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
    
    /* Enable USB HS Clocks */
    __USB_OTG_HS_CLK_ENABLE();
    __USB_OTG_HS_ULPI_CLK_ENABLE();
#endif // !USE_USB_HS_IN_FS
    
    /* Set USBHS Interrupt to the lowest priority */
    HAL_NVIC_SetPriority(OTG_HS_IRQn, IRQ_PRI_OTG_HS, IRQ_SUBPRI_OTG_HS);
    
    /* Enable USBHS Interrupt */
    HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
  }   
#endif  // USE_USB_HS
}
/**
  * @brief  DeInitializes the PCD MSP.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd)
{
  if(hpcd->Instance == USB_OTG_FS)
  {  
    /* Disable USB FS Clocks */ 
    __USB_OTG_FS_CLK_DISABLE();
    __SYSCFG_CLK_DISABLE(); 
  }
  #if defined(USE_USB_HS)
  else if(hpcd->Instance == USB_OTG_HS)
  {  
    /* Disable USB FS Clocks */ 
    __USB_OTG_HS_CLK_DISABLE();
    __SYSCFG_CLK_DISABLE(); 
  }  
  #endif
}

/*******************************************************************************
                       LL Driver Callbacks (PCD -> USB Device Library)
*******************************************************************************/


/**
  * @brief  Setup stage callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup);
}

/**
  * @brief  Data Out stage callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number
  * @retval None
  */
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

/**
  * @brief  Data In stage callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number
  * @retval None
  */
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

/**
  * @brief  SOF callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
/*
This is now handled by the USB CDC interface.
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_SOF(hpcd->pData);
}
*/

/**
  * @brief  Reset callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{ 
  USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

  /* Set USB Current Speed */
  switch(hpcd->Init.speed)
  {
#if defined(PCD_SPEED_HIGH)
  case PCD_SPEED_HIGH:
    speed = USBD_SPEED_HIGH;
    break;
#endif
    
  case PCD_SPEED_FULL:
    speed = USBD_SPEED_FULL;    
    break;
	
	default:
    speed = USBD_SPEED_FULL;    
    break;
  }
  USBD_LL_SetSpeed(hpcd->pData, speed);  
  
  /* Reset Device */
  USBD_LL_Reset(hpcd->pData);
}

/**
  * @brief  Suspend callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_Suspend(hpcd->pData);
}

/**
  * @brief  Resume callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_Resume(hpcd->pData);
}

/**
  * @brief  ISOC Out Incomplete callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number  
  * @retval None
  */
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_IsoOUTIncomplete(hpcd->pData, epnum);
}

/**
  * @brief  ISOC In Incomplete callback.
  * @param  hpcd: PCD handle
  * @param  epnum: Endpoint Number  
  * @retval None
  */
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_IsoINIncomplete(hpcd->pData, epnum);
}

/**
  * @brief  Connect callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_DevConnected(hpcd->pData);
}

/**
  * @brief  Disconnect callback.
  * @param  hpcd: PCD handle
  * @retval None
  */
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_DevDisconnected(hpcd->pData);
}

/*******************************************************************************
                       LL Driver Interface (USB Device Library --> PCD)
*******************************************************************************/
/**
  * @brief  Initializes the Low Level portion of the Device driver.  
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef  USBD_LL_Init (USBD_HandleTypeDef *pdev)
{ 
#if defined(USE_USB_FS)
if (pdev->id ==  USB_PHY_FS_ID)
{
  /*Set LL Driver parameters */
  pcd_fs_handle.Instance = USB_OTG_FS;
  pcd_fs_handle.Init.dev_endpoints = 4;
  pcd_fs_handle.Init.use_dedicated_ep1 = 0;
  pcd_fs_handle.Init.ep0_mps = 0x40;
  pcd_fs_handle.Init.dma_enable = 0;
  pcd_fs_handle.Init.low_power_enable = 0;
  pcd_fs_handle.Init.phy_itface = PCD_PHY_EMBEDDED;
  pcd_fs_handle.Init.Sof_enable = 1;
  pcd_fs_handle.Init.speed = PCD_SPEED_FULL;
#if defined(MCU_SERIES_L4)
  pcd_fs_handle.Init.lpm_enable = DISABLE;
  pcd_fs_handle.Init.battery_charging_enable = DISABLE;
#endif
#if !defined(MICROPY_HW_USB_VBUS_DETECT_PIN)
  pcd_fs_handle.Init.vbus_sensing_enable = 0; // No VBUS Sensing on USB0
#else
  pcd_fs_handle.Init.vbus_sensing_enable = 1;
#endif
  /* Link The driver to the stack */
  pcd_fs_handle.pData = pdev;
  pdev->pData = &pcd_fs_handle;
  /*Initialize LL Driver */
  HAL_PCD_Init(&pcd_fs_handle);

  HAL_PCD_SetRxFiFo(&pcd_fs_handle, 0x80);
  HAL_PCD_SetTxFiFo(&pcd_fs_handle, 0, 0x20);
  HAL_PCD_SetTxFiFo(&pcd_fs_handle, 1, 0x40);
  HAL_PCD_SetTxFiFo(&pcd_fs_handle, 2, 0x20);
  HAL_PCD_SetTxFiFo(&pcd_fs_handle, 3, 0x40);
}
#endif
#if defined(USE_USB_HS)
if (pdev->id == USB_PHY_HS_ID)
{
#if defined(USE_USB_HS_IN_FS)
  /*Set LL Driver parameters */
  pcd_hs_handle.Instance = USB_OTG_HS;
  pcd_hs_handle.Init.dev_endpoints = 4;
  pcd_hs_handle.Init.use_dedicated_ep1 = 0;
  pcd_hs_handle.Init.ep0_mps = 0x40;
  pcd_hs_handle.Init.dma_enable = 0;
  pcd_hs_handle.Init.low_power_enable = 0;
  pcd_hs_handle.Init.phy_itface = PCD_PHY_EMBEDDED;
  pcd_hs_handle.Init.Sof_enable = 1;
  pcd_hs_handle.Init.speed = PCD_SPEED_HIGH_IN_FULL;
#if !defined(MICROPY_HW_USB_VBUS_DETECT_PIN)
  pcd_hs_handle.Init.vbus_sensing_enable = 0; // No VBUS Sensing on USB0
#else
  pcd_hs_handle.Init.vbus_sensing_enable = 1;
#endif
  /* Link The driver to the stack */
  pcd_hs_handle.pData = pdev;
  pdev->pData = &pcd_hs_handle;
  /*Initialize LL Driver */
  HAL_PCD_Init(&pcd_hs_handle);

  HAL_PCD_SetRxFiFo(&pcd_hs_handle, 0x80);
  HAL_PCD_SetTxFiFo(&pcd_hs_handle, 0, 0x20);
  HAL_PCD_SetTxFiFo(&pcd_hs_handle, 1, 0x40);
  HAL_PCD_SetTxFiFo(&pcd_hs_handle, 2, 0x20);
  HAL_PCD_SetTxFiFo(&pcd_hs_handle, 3, 0x40);
#else // !defined(USE_USB_HS_IN_FS)
  /*Set LL Driver parameters */
  pcd_hs_handle.Instance = USB_OTG_HS;
  pcd_hs_handle.Init.dev_endpoints = 6;
  pcd_hs_handle.Init.use_dedicated_ep1 = 0;
  pcd_hs_handle.Init.ep0_mps = 0x40;
  
  /* Be aware that enabling USB-DMA mode will result in data being sent only by
     multiple of 4 packet sizes. This is due to the fact that USB-DMA does
     not allow sending data from non word-aligned addresses.
     For this specific application, it is advised to not enable this option
     unless required. */
  pcd_hs_handle.Init.dma_enable = 0;
  
  pcd_hs_handle.Init.low_power_enable = 0;
  pcd_hs_handle.Init.phy_itface = PCD_PHY_ULPI;
  pcd_hs_handle.Init.Sof_enable = 1;
  pcd_hs_handle.Init.speed = PCD_SPEED_HIGH;
  pcd_hs_handle.Init.vbus_sensing_enable = 1;
  /* Link The driver to the stack */
  pcd_hs_handle.pData = pdev;
  pdev->pData = &pcd_hs_handle;
  /*Initialize LL Driver */
  HAL_PCD_Init(&pcd_hs_handle);
  
  HAL_PCD_SetRxFiFo(&pcd_hs_handle, 0x200);
  HAL_PCD_SetTxFiFo(&pcd_hs_handle, 0, 0x80);
  HAL_PCD_SetTxFiFo(&pcd_hs_handle, 1, 0x174);

#endif  // !USE_USB_HS_IN_FS
}
#endif  // USE_USB_HS
  return USBD_OK;
}

/**
  * @brief  De-Initializes the Low Level portion of the Device driver. 
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  HAL_PCD_DeInit(pdev->pData);
  return USBD_OK; 
}

/**
  * @brief  Starts the Low Level portion of the Device driver.    
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  HAL_PCD_Start(pdev->pData);
  return USBD_OK; 
}

/**
  * @brief  Stops the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  HAL_PCD_Stop(pdev->pData);
  return USBD_OK; 
}

/**
  * @brief  Opens an endpoint of the Low Level Driver. 
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  ep_type: Endpoint Type
  * @param  ep_mps: Endpoint Max Packet Size                 
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, 
                                  uint8_t  ep_addr,                                      
                                  uint8_t  ep_type,
                                  uint16_t ep_mps)
{
  HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);
  return USBD_OK; 
}

/**
  * @brief  Closes an endpoint of the Low Level Driver.  
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)   
{
  HAL_PCD_EP_Close(pdev->pData, ep_addr);
  return USBD_OK; 
}

/**
  * @brief  Flushes an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)   
{
  HAL_PCD_EP_Flush(pdev->pData, ep_addr);
  return USBD_OK; 
}

/**
  * @brief  Sets a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)   
{
  HAL_PCD_EP_SetStall(pdev->pData, ep_addr);
  return USBD_OK; 
}

/**
  * @brief  Clears a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)   
{
  HAL_PCD_EP_ClrStall(pdev->pData, ep_addr);  
  return USBD_OK; 
}

/**
  * @brief  Returns Stall condition.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number      
  * @retval Stall (1: yes, 0: No)
  */
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)   
{
  PCD_HandleTypeDef *hpcd = pdev->pData; 
  
  if((ep_addr & 0x80) == 0x80)
  {
    return hpcd->IN_ep[ep_addr & 0x7F].is_stall; 
  }
  else
  {
    return hpcd->OUT_ep[ep_addr & 0x7F].is_stall; 
  }
}

/**
  * @brief  Assigns an USB address to the device
  * @param  pdev: Device handle
  * @param  dev_addr: USB address      
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)   
{
  HAL_PCD_SetAddress(pdev->pData, dev_addr);
  return USBD_OK; 
}

/**
  * @brief  Transmits data over an endpoint 
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  pbuf: Pointer to data to be sent    
  * @param  size: Data size    
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, 
                                    uint8_t  ep_addr,                                      
                                    uint8_t  *pbuf,
                                    uint16_t  size)
{
  HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size);
  return USBD_OK;   
}

/**
  * @brief  Prepares an endpoint for reception 
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  pbuf:pointer to data to be received    
  * @param  size: data size              
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, 
                                          uint8_t  ep_addr,                                      
                                          uint8_t  *pbuf,
                                          uint16_t  size)
{
  HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size);
  return USBD_OK;   
}

/**
  * @brief  Returns the last transfered packet size.    
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval Recived Data Size
  */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t  ep_addr)  
{
  return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr);
}

/**
  * @brief  Delay routine for the USB Device Library      
  * @param  Delay: Delay in ms
  * @retval None
  */
void  USBD_LL_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);  
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
