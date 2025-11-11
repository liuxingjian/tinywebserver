#include<iostream>
#include<functional>
int add(int a,int b){
    return a+b;
}

int main(){
    auto bound_add=std::bind(add,5, std::placeholders::_1);
    std::cout<<bound_add(10)<<std::endl;
    return 0;
}