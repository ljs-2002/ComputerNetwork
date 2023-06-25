'''
- 在无第三方参与的前提下
- 实现Base64编码表的映射关系自动 搜索、一次一变
'''
import random
import hashlib

class myBase64:
    def __init__(self,initBase64Map:str) -> None:
        self.initBase64Map = initBase64Map
        self.base64Map = self.initBase64Map
        self.base64MapLen = len(self.base64Map)

    def __getNewBase64Map(self,last:str)->None:
        '''
        - 生成新的base64编码表
        - last:上一次处理的分组
        - return:新的base64编码表
        '''
        #计算上次处理的6位字符的hash值，转换为int类型
        lastHash = int(hashlib.sha256(last.encode('utf-8')).hexdigest(),16)
        newBase64Map = list(self.base64Map)
        #以该哈希值为种子
        random.seed(lastHash%2147483647)
        #随机生成交换的位置
        steps = [random.randint(1, self.base64MapLen - 1) for _ in range(self.base64MapLen)]
        #交换每个位置上的映射值
        for i in range(self.base64MapLen):
            new_pos = (i + steps[i]) % self.base64MapLen
            newBase64Map[i], newBase64Map[new_pos] = newBase64Map[new_pos], newBase64Map[i]
        #得到新的映射表
        newBase64Map = ''.join(newBase64Map)
        self.base64Map = newBase64Map

    def __base64Encoder(self,group:str)->str:
        '''
        - 使用自定义的base64编码表进行编码
        - group:分组
        - return:编码后的字符
        '''
        encodedStr = self.base64Map[int(group,2)]
        self.__getNewBase64Map(group)
        return encodedStr

    def __base64Decoder(self,group:str)->str:
        '''
        - 使用自定义的base64编码表进行解码
        - group:分组
        - return:解码后的6位2进展字符串
        '''
        decodedStr = bin(self.base64Map.index(group))[2:].zfill(6)
        self.__getNewBase64Map(decodedStr)
        return decodedStr

    def encoder(self,s:str):
        # 将字符串转换为字节类型
        b = s.encode('utf-8')
        # 将字节类型转换为二进制字符串
        binary_str = ''.join(format(byte, '08b') for byte in b)
        # 将二进制字符串按6位一组分割，并在末尾补0
        binary_str += '0' * (6 - len(binary_str) % 6)
        groups = [binary_str[i:i+6] for i in range(0, len(binary_str), 6)]
        # 根据编码表将每组转换为对应的字符
        encoded_str = ''.join(self.__base64Encoder(group) for group in groups)
        # 在末尾补上等号
        padding = '=' * ((4 - len(encoded_str) % 4) % 4)
        return encoded_str + padding
    
    def decoder(self,s:str):
        # 去除末尾的等号
        s = s.rstrip('=')
        # 将字符转换为对应的数字
        groups = "".join(self.__base64Decoder(group) for group in s)
        # 将二进制字符串按8位一组分割，并将其转换为字节类型
        bytes_list = [int(groups[i:i+8], 2) for i in range(0, len(groups), 8)]
        b = bytes(bytes_list)
        # 将字节类型转换为字符串
        return b.decode('utf-8')