let imageWorker = null
let originalImageData = null
let imageWidth = 0
let imageHeight = 0
let imageChannels = 4 // RGBA
let processingQueue = new Map() // Track pending operations


function showStatus(message, type = "info") {
  if (message === "") {
    const status = document.getElementById("status")
    status.style.display = "none"
    return
  }

  const status = document.getElementById("status")
  status.textContent = message
  status.className = `status ${type}`
  status.style.display = "block"

  // Auto-hide after 3 seconds for info messages
  if (type === "info") {
    setTimeout(() => {
      status.style.display = "none"
    }, 3000)
  }
}

// Initialize Web Worker
function initImageWorker() {
  try {
    showStatus("Loading FlatCV WebAssembly module...", "info")
    imageWorker = new Worker("image-worker.js")

    imageWorker.onmessage = (e) => {
      const { type, success, message, data, id } = e.data

      if (type === "init") {
        if (success) {
          showStatus("")
          loadDefaultImage()
        }
        else {
          showStatus("Failed to load FlatCV module: " + message, "error")
        }
      }
      else if (type === "result") {
        const operation = processingQueue.get(id)
        if (operation) {
          processingQueue.delete(id)

          if (success) {
            displayProcessedImage(data.processedData, data.width, data.height, data.channels)
          }
          else {
            showStatus(`Failed to apply ${operation.name.toLowerCase()}: ${message}`, "error")
          }

          hideSpinner(operation.spinnerId)
        }
      }
    }

    imageWorker.onerror = (error) => {
      showStatus("Worker error: " + error.message, "error")
      console.error("Worker error:", error)
    }

    // Initialize the worker
    imageWorker.postMessage({ type: "init" })
  }
  catch (error) {
    showStatus("Failed to create worker: " + error.message, "error")
    console.error("Error creating worker:", error)
  }
}

function showSpinner(spinnerId) {
  const spinner = document.getElementById(spinnerId)
  if (spinner) {
    spinner.style.display = "block"
  }
}

function hideSpinner(spinnerId) {
  const spinner = document.getElementById(spinnerId)
  if (spinner) {
    spinner.style.display = "none"
  }
}

function enableFilterButtons() {
  document.querySelectorAll(".filter-btn").forEach(btn => {
    btn.disabled = false
  })
}

function disableFilterButtons() {
  document.querySelectorAll(".filter-btn").forEach(btn => {
    btn.disabled = true
  })
}

// Load image from file
document.getElementById("loadImageBtn").addEventListener("click", () => {
  document.getElementById("fileInput").click()
})

document.getElementById("fileInput").addEventListener("change", (event) => {
  const file = event.target.files[0]
  if (!file) return

  showSpinner("loadSpinner")

  const reader = new FileReader()
  reader.onload = (e) => {
    const img = new Image()
    img.onload = () => {
      const canvas = document.getElementById("originalCanvas")
      const ctx = canvas.getContext("2d")

      canvas.width = img.width
      canvas.height = img.height
      ctx.drawImage(img, 0, 0)

      // Get image data
      originalImageData = ctx.getImageData(0, 0, img.width, img.height)
      imageWidth = img.width
      imageHeight = img.height

      enableFilterButtons()
      hideSpinner("loadSpinner")
    }
    img.src = e.target.result
  }
  reader.readAsDataURL(file)
})

// Helper function to process image using worker
function processImageWithWorker(operation, spinnerId, operationName) {
  if (!imageWorker || !originalImageData) return

  const operationId = Date.now() + Math.random()

  // Track this operation
  processingQueue.set(operationId, {
    name: operationName,
    spinnerId: spinnerId
  })

  showSpinner(spinnerId)

  // Send image data to worker
  imageWorker.postMessage({
    type: "process",
    operation: operation,
    id: operationId,
    data: {
      imageData: originalImageData.data,
      width: imageWidth,
      height: imageHeight
    }
  })
}

// Display processed image
function displayProcessedImage(processedData, width, height, channels) {
  const canvas = document.getElementById("processedCanvas")
  const ctx = canvas.getContext("2d")

  canvas.width = width
  canvas.height = height

  const imageData = ctx.createImageData(width, height)

  if (channels === 1) {
    // Grayscale - convert to RGBA
    for (let i = 0; i < width * height; i++) {
      const gray = processedData[i]
      imageData.data[i * 4] = gray // R
      imageData.data[i * 4 + 1] = gray // G
      imageData.data[i * 4 + 2] = gray // B
      imageData.data[i * 4 + 3] = 255 // A
    }
  }
  else if (channels === 4) {
    // RGBA
    imageData.data.set(processedData)
  }

  ctx.putImageData(imageData, 0, 0)
}

// Filter functions using Web Worker
document.getElementById("grayscaleBtn").addEventListener("click", () => {
  processImageWithWorker("grayscale", "grayscaleSpinner", "Grayscale filter")
})

document.getElementById("blurBtn").addEventListener("click", () => {
  processImageWithWorker("blur", "blurSpinner", "Blur filter")
})

document.getElementById("sobelBtn").addEventListener("click", () => {
  processImageWithWorker("sobel", "sobelSpinner", "Edge detection")
})

document.getElementById("binaryBtn").addEventListener("click", () => {
  processImageWithWorker("binary", "binarySpinner", "Binary threshold")
})

function loadDefaultImage() {
  const img = new Image()
  img.onload = () => {
    const canvas = document.getElementById("originalCanvas")
    const ctx = canvas.getContext("2d")

    canvas.width = img.width
    canvas.height = img.height
    ctx.drawImage(img, 0, 0)

    // Get image data
    originalImageData = ctx.getImageData(0, 0, img.width, img.height)
    imageWidth = img.width
    imageHeight = img.height

    enableFilterButtons()
  }
  img.onerror = () => {
    showStatus("Failed to load default image", "error")
  }
  img.src = "parrot.jpeg"
}

// Initialize when page loads
window.addEventListener("load", initImageWorker)
