from mybase64 import myBase64
from tqdm import tqdm
if __name__ == '__main__':
    initBase64Map = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
    myBase64Encode = myBase64(initBase64Map)
    myBase64Decode = myBase64(initBase64Map)
    string1 = '你好abcde'
    print(f"[+]检验是否能够成功编码和解码，原始字符串：{string1}")
    encodedStr1 = myBase64Encode.encoder(string1)
    encodedStr2 = myBase64Encode.encoder(string1)
    decodedStr1 = myBase64Decode.decoder(encodedStr1)
    decodedStr2 = myBase64Decode.decoder(encodedStr2)
    print("    第一次编码、解码：",encodedStr1,decodedStr1)
    print("    第二次编码、解码：",encodedStr2,decodedStr2)
    size = 390
    print(f"[+]重复编码{size}次，检验编码后得到的字符串是否会出现重复")
    encodedStrList = []
    for i in tqdm(range(size),desc="正在测试",total=size):
        encodedStr = myBase64Encode.encoder(string1)
        encodedStrList.append(encodedStr)
    encodedStrList = list(set(encodedStrList))
    if(len(encodedStrList) == size):
        print('    编码表没有重复')
    else:
        print('    编码表有重复')