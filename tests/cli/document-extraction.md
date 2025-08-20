## Document Extraction

### Extract Document with Auto-Sizing

Input | Output
------|--------
![](imgs/receipt.jpeg) | ![](imgs/receipt_extracted_auto.jpeg)

```scrut
$ ./flatcv imgs/receipt.jpeg extract_document imgs/receipt_extracted_auto.jpeg
Loaded image: 1024x1024 with 3 channels
Executing pipeline with 1 operations:
Applying operation: extract_document
  → Completed in \d+.\d+ ms \(output: \d+x\d+\) (regex)
Final output dimensions: \d+x\d+ (regex)
Successfully saved processed image to 'imgs/receipt_extracted_auto.jpeg'
```

Input | Output
------|--------
![](imgs/receip2.jpeg) | ![](imgs/receipt2_extracted_auto.jpeg)

```scrut
$ ./flatcv imgs/receipt2.jpeg extract_document imgs/receipt2_extracted_auto.jpeg
Loaded image: 1024x1024 with 3 channels
Executing pipeline with 1 operations:
Applying operation: extract_document
  → Completed in \d+.\d+ ms \(output: \d+x\d+\) (regex)
Final output dimensions: \d+x\d+ (regex)
Successfully saved processed image to 'imgs/receipt2_extracted_auto.jpeg'
```


### Extract Document to Specific Dimensions

Input | Output
------|--------
![](imgs/receipt.jpeg) | ![](imgs/receipt_extracted.jpeg)

```scrut
$ ./flatcv imgs/receipt.jpeg extract_document_to 200x400 imgs/receipt_extracted.jpeg
Loaded image: 1024x1024 with 3 channels
Executing pipeline with 1 operations:
Applying operation: extract_document_to with parameter: 200.00 400.00
  → Completed in \d+.\d+ ms \(output: 200x400\) (regex)
Final output dimensions: 200x400
Successfully saved processed image to 'imgs/receipt_extracted.jpeg'
```
