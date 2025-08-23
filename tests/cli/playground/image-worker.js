// Web Worker for image processing operations
let flatcvModule = null

// Load FlatCV WebAssembly module
async function initFlatCV() {
  try {
    // Import the FlatCV module
    importScripts("flatcv.js")
    flatcvModule = await FlatCV()
    postMessage({ type: "init", success: true, message: "FlatCV module loaded successfully!" })
  }
  catch (error) {
    postMessage({ type: "init", success: false, message: "Failed to load FlatCV module: " + error.message })
  }
}

// Helper function to copy data to WebAssembly memory
function copyToWasmMemory(data) {
  const dataPtr = flatcvModule._malloc(data.length)
  flatcvModule.HEAPU8.set(data, dataPtr)
  return dataPtr
}

// Helper function to copy data from WebAssembly memory
function copyFromWasmMemory(ptr, length) {
  return new Uint8Array(flatcvModule.HEAPU8.buffer.slice(ptr, ptr + length))
}

// Process image operations
async function processImage(operation, data) {
  if (!flatcvModule) {
    throw new Error("FlatCV module not initialized")
  }

  const { imageData, width, height } = data
  const inputPtr = copyToWasmMemory(imageData)
  let outputPtr = null
  let processedData = null
  let channels = 4

  try {
    switch (operation) {
      case "grayscale":
        outputPtr = flatcvModule.ccall("fcv_grayscale",
          "number",
          ["number", "number", "number"],
          [width, height, inputPtr]
        )
        if (outputPtr) {
          processedData = copyFromWasmMemory(outputPtr, width * height * 4)
          channels = 4
        }
        break

      case "blur":
        outputPtr = flatcvModule.ccall("fcv_apply_gaussian_blur",
          "number",
          ["number", "number", "number", "number"],
          [width, height, 5.0, inputPtr]
        )
        if (outputPtr) {
          processedData = copyFromWasmMemory(outputPtr, width * height * 4)
          channels = 4
        }
        break

      case "sobel":
        outputPtr = flatcvModule.ccall("fcv_sobel_edge_detection",
          "number",
          ["number", "number", "number", "number"],
          [width, height, 4, inputPtr]
        )
        if (outputPtr) {
          processedData = copyFromWasmMemory(outputPtr, width * height)
          channels = 1
        }
        break

      case "binary":
        outputPtr = flatcvModule.ccall("fcv_otsu_threshold_rgba",
          "number",
          ["number", "number", "number", "number"],
          [width, height, 0, inputPtr]
        )
        if (outputPtr) {
          processedData = copyFromWasmMemory(outputPtr, width * height * 4)
          channels = 4
        }
        break

      default:
        throw new Error("Unknown operation: " + operation)
    }

    if (!outputPtr || !processedData) {
      throw new Error("Failed to process image")
    }

    return {
      processedData: processedData,
      width: width,
      height: height,
      channels: channels
    }

  } finally {
    // Clean up memory
    flatcvModule._free(inputPtr)
    if (outputPtr) {
      flatcvModule._free(outputPtr)
    }
  }
}

// Handle messages from main thread
self.onmessage = async function(e) {
  const { type, operation, data, id } = e.data

  try {
    if (type === "init") {
      await initFlatCV()
    }
    else if (type === "process") {
      const result = await processImage(operation, data)
      postMessage({
        type: "result",
        id: id,
        success: true,
        data: result
      })
    }
  }
  catch (error) {
    postMessage({
      type: type === "init" ? "init" : "result",
      id: id,
      success: false,
      message: error.message
    })
  }
}
