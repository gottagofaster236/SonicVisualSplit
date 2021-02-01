from PIL import Image
import os

directory = "./Sonic 1@RGB"
output_directory = "./Sonic 1@Composite"

for file in os.listdir(directory):
    path = os.path.join(directory, file)
    print(path)
    im = Image.open(path)
    im = im.convert('RGBA')
    width, height = im.size
    result_im = Image.new('RGBA', (width - 1, height))
    
    for x in range(0, width - 1):
        for y in range(height):
            pixel1 = im.getpixel((x, y))
            pixel2 = im.getpixel((x + 1, y))
                
            result = None
            if pixel1[3] == 0 or pixel2[3] == 0:
                result = (0, 0, 0, 0)
            else:
                result = ((pixel1[0] + pixel2[0]) // 2, (pixel1[1] + pixel2[1]) // 2, (pixel1[2] + pixel2[2]) // 2, 255)
            result_im.putpixel((x, y), result)
            
    result_im.save(os.path.join(output_directory, file))
