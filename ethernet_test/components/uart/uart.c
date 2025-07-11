#include <stdio.h>
#include "driver/uart.h"
#include <string.h>
#include "esp_log.h"
#include "uart.h"
#include "buffer.h"
#include "priority.h"
#include "format.h"

#define UART_NUM UART_NUM_0
#define TX_PIN 1 // GPIO pin number for TX (transmit)
#define RX_PIN 3 // GPIO pin number for RX (receive)
#define UART_BUF_SIZE 1500 // Size of the buffer to store received data

static const char *TAG = "UART"; // Tag for logging

uint8_t* generate_data(int type){

    uint8_t* data = (uint8_t*)malloc(28); // Allocate memory for the data buffer
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for data buffer");
        return NULL; // Return NULL if memory allocation fails
    }

    if (type == 0){
        data[0] = 0b10101000;
    } else if (type == 1){
        data[0] = 0b10101001; // Set the first byte to indicate bit-flip telemetry
    } else if (type == 2){
        data[0] = 0b10101010; // Set the first byte to indicate radiation telemetry
    } else if (type == 3){
        data[0] = 0b10101011; // Set the first byte to indicate acknowledgement telemetry
    } else {
        ESP_LOGE(TAG, "Invalid telemetry type: %d", type);
        free(data); // Free allocated memory if type is invalid
        return (uint8_t*)NULL; // Return NULL if type is invalid
    }
    
    for (int i = 1; i < 28; i++) { // Fill the rest of the data buffer with dummy data
        data[i] = i; // Just an example, you can fill it with actual telemetry data
    }

    return data; // Return the generated data buffer

}

void uart_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting UART task"); // Log the start of the UART task
    int test = 0;
    while(1) {
        // Check if there is any tc in buffer
        uint8_t* tc_data = buffer_retreive_tc(); // Retrieve data from the buffer
        if (tc_data != NULL) { // Check if there is data in the buffer
            uart_send(tc_data); // Send the retrieved data over UART
        }
        
        // uint8_t* data = uart_receive(); // Receive data from UART
        uint8_t* data = generate_data(test%2); // Generate dummy data for testing, replace with uart_receive() in production

        if (data == NULL) {
            continue; // Continue to the next iteration of the loop
        }
        // Send to priority
        int priority = priority_assign(data); // Check the priority of the received data
        // Send to format
        uint8_t* formatted_data = format_tm(data); // Format the received data based on telemetry type
        if (formatted_data == NULL) {
            ESP_LOGE(TAG, "Failed to format data");
            free(data); // Free allocated memory if formatting fails
            continue; // Continue to the next iteration of the loop
        }
        int i;
        for (i = 0; i < 34; i++)
        {
            if (i > 0) printf(":");
            printf("%02X", formatted_data[i]);
        }
        printf("\n");
        // Send to buffer
        buffer_add_tm(priority, formatted_data); // Add the received data to the buffer with the assigned priority

        test++;
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second before the next iteration
    }
}


/// @brief Function to check the length of the data based on telemetry type
/// @param data: Pointer to the data buffer (first byte of the data) 
/// @return int length of the data to be read
int check_length(uint8_t* data) {
    int telemetry_type = CHECK_BIT(data[0], 0) + CHECK_BIT(data[0], 1) * 2; // Get the telemetry type from the first byte

    if (telemetry_type == 0){
        // ESP_LOGI(TAG,"Telemetry type: housekeeping"); // Log telemetry type
        return 28; // Should return the length of the data to be read (look in sed or somewhere)
    } 
    if (telemetry_type == 1){
        // ESP_LOGI(TAG,"Telemetry type: bit-flip"); // Log telemetry type
        return 28; // Should return the length of the data to be read (look in sed or somewhere)
    }
    if (telemetry_type == 2){
        // ESP_LOGI(TAG,"Telemetry type: radiation"); // Log telemetry type
        return 3; // Should return the length of the data to be read (This ones wierd cuz don't really know size)
    }
    if (telemetry_type == 3){
        // ESP_LOGI(TAG,"Telemetry type: acknowledgement"); // Log telemetry type
        return 4; // Should return the length of the data to be read (look in sed or somewhere)
    }

    ESP_LOGW(TAG,"No telemetry type found, returning 1500 bytes"); // Log warning message
    return 1500; // Return default size if no telemetry type is found

}

/// @brief Function to concatenate two data buffers
/// @param data1: Pointer to the first data buffer
/// @param data2: Pointer to the second data buffer
/// @param length: Length of the concatenated data buffer
/// @return Pointer to the concatenated data buffer (should be freed after use)
uint8_t* concatenate_data(uint8_t* data1, uint8_t* data2, int length) {
    
    uint8_t* concatenated_data = (uint8_t*)malloc(length); // Allocate memory for concatenated data
    if (!concatenated_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for concatenated data");
        return (uint8_t*)NULL; // Return NULL if memory allocation fails
    }

    memcpy(concatenated_data, data1, 1); // Copy first data buffer to concatenated data
    memcpy(concatenated_data + 1, data2, length - 1); // Copy second data buffer to concatenated data

    return concatenated_data; // Return the concatenated data
}


void uart_init(void) {

    uart_flush(UART_NUM); // Flush the UART buffer to clear any existing data

    // Configure the UART parameters
    uart_config_t uart_config = {
        .baud_rate = 115200,                // Set baud rate
        .data_bits = UART_DATA_8_BITS,      // Set data bits
        .parity = UART_PARITY_DISABLE,      // Disable parity
        .stop_bits = UART_STOP_BITS_1,      // Set stop bits
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  // Disable flow control
        .source_clk = UART_SCLK_APB         // Set source clock
    };

    esp_err_t ret = ESP_OK; // Variable to store return value

    // Initialize the UART with the configuration
    ret = uart_param_config(UART_NUM, &uart_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "UART configured successfully");
    } else {
        ESP_LOGE(TAG, "Failed to configure UART");
    }

    // Set UART pins (TX and RX)
    ret = uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "UART pins set successfully");
    } else {
        ESP_LOGE(TAG, "Failed to set UART pins");
    }
    
    // Install the UART driver
    ret = uart_driver_install(UART_NUM, 2048, 0, 0, NULL, 0);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "UART driver installed successfully");
    } else {
        ESP_LOGE(TAG, "Failed to install UART driver");
    }

    xTaskCreate(&uart_task, "uart_task", 2048, NULL, 10, NULL); // Create a task for UART data reception

}


void uart_send(uint8_t* message) {
    // Send the message over UART
    ESP_LOGI(TAG, "Sending message: %s, length :%d", message, check_length(message)); // Log the message being sent
    uart_write_bytes(UART_NUM, message, check_length(message)); // Send the message with the length determined by check_length function
    uart_write_bytes(UART_NUM, "\n", 1); // Send a newline character after the message
}


uint8_t* uart_receive(void) {

    uint8_t header;

    int length = uart_read_bytes(UART_NUM, &header, 1, pdMS_TO_TICKS(20)); // Read one byte of data from UART
    // ESP_LOGI(TAG, "Received byte: %02X", header); // Log the received byte in hexadecimal format
    if (length != 1) { // Ensure exactly one byte is read
        if (length < 0) {
            ESP_LOGE(TAG, "Error reading bytes from UART"); // Log error message
        } else {
            // ESP_LOGW(TAG, "No data received"); // Log warning for no data
        }
        return (uint8_t*)NULL; // Return NULL in case of error or unexpected byte count
    }
    
    int tm_length = check_length(&header); // Check the length of the data to be read based on telemetry type
    // ESP_LOGI(TAG, "Telemetry length: %d", tm_length); // Log the telemetry length
    if (tm_length <= 0 || tm_length > UART_BUF_SIZE) {
        // ESP_LOGE(TAG, "Invalid telemetry length: %d", tm_length); // Log error message for invalid telemetry length
        return (uint8_t*)NULL; // Return NULL if telemetry length is invalid
    }

    uint8_t* full_data = (uint8_t*)malloc(tm_length); // Dynamically allocate memory for the data buffer (-1 to account for the full tm length with the temp variable too)
    // uint8_t* full_data = (uint8_t*)malloc(1);
    
    if (full_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for data buffer");
        return (uint8_t*)NULL; // Return NULL if memory allocation fails
    }

    full_data[0] = header;

    // ESP_LOGI(TAG, "Bytes of char array: %d", strlen((const char*)full_data)); // Log the size of the char array

    uart_read_bytes(UART_NUM, full_data + 1, tm_length-1, pdMS_TO_TICKS(20)); // Read data from UART
    
    if (full_data[tm_length] != '\0') { // Check if the last byte is not null terminated
        full_data[tm_length] = '\0'; // Null terminate the data buffer
    }
    // ESP_LOGI(TAG, "Received data: %s", full_data); // Log the received data
    // ESP_LOGI(TAG, "Bytes of char array: %d", strlen((const char*)full_data)); // Log the size of the char array

    return full_data; // Return the dynamically allocated data
}