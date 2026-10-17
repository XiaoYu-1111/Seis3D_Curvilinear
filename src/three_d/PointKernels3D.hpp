#pragma once
// The CPU and CUDA execute these same Float64 point operators. No owning STL storage here.
#include <cmath>
#include <cstddef>
#include "Seis3D/KernelTypes3D.hpp"
#include "SeisCommon/FiniteDifference.hpp"
#ifdef __CUDACC__
#define SEIS_POINT __host__ __device__ inline
#else
#define SEIS_POINT inline
#endif
namespace Seis3D::Curvilinear::Point {
struct Mesh {
    int nx{},ny{},nz{};
    std::size_t count{};
    double h{},lambda{},mu{},density{};
    const double *metrics{},*normals{},*jacobian{},*source{},*damping{};
    const double *materialLambda{},*materialMu{},*materialDensity{};
    double sourceDirection[3]{0,0,1},sourceAmplitude{2e8};
    const double *sourceGradient[3]{};
};
SEIS_POINT double localLambda(const Mesh& g,std::size_t q){return g.materialLambda?g.materialLambda[q]:g.lambda;}
SEIS_POINT double localMu(const Mesh& g,std::size_t q){return g.materialMu?g.materialMu[q]:g.mu;}
SEIS_POINT double localDensity(const Mesh& g,std::size_t q){return g.materialDensity?g.materialDensity[q]:g.density;}
struct Fields {double* f[9]{};};
// fd_t.c FD_Flags: step-dependent direction pairs, reversed at EVERY RK substage.
SEIS_POINT int direction(int step,int stage,int axis){return SeisWave::Difference::direction(step,stage,axis);}
SEIS_POINT int stencil(int dir,int depth,int* offsets,double* coefficients){return SeisWave::Difference::stencil(dir,depth,offsets,coefficients);}
SEIS_POINT std::size_t index(const Mesh& g,int i,int j,int k) {return i+static_cast<std::size_t>(g.nx)*(j+static_cast<std::size_t>(g.ny)*k);}
SEIS_POINT double derivative(const double* v,const Mesh& g,std::size_t q,int axis,int dir,int depth=-1) {
    const std::ptrdiff_t s=axis==0?1:axis==1?g.nx:static_cast<std::ptrdiff_t>(g.nx)*g.ny;
    int offsets[5];double coefficients[5];const int n=stencil(dir,depth,offsets,coefficients);
    double result=0;for(int l=0;l<n;++l)result+=coefficients[l]*v[static_cast<std::ptrdiff_t>(q)+offsets[l]*s];
    return result/g.h;
}
SEIS_POINT void stress(const Fields& w,std::size_t q,double* s) {
    s[0]=w.f[3][q];s[4]=w.f[4][q];s[8]=w.f[5][q];
    s[1]=s[3]=w.f[6][q];s[2]=s[6]=w.f[7][q];s[5]=s[7]=w.f[8][q];
}
SEIS_POINT void storeStress(Fields w,std::size_t q,const double* s) {
    w.f[3][q]=s[0];w.f[4][q]=s[4];w.f[5][q]=s[8];
    w.f[6][q]=0.5*(s[1]+s[3]);w.f[7][q]=0.5*(s[2]+s[6]);w.f[8][q]=0.5*(s[5]+s[7]);
}
SEIS_POINT void tangentStress(const double* s,const double* n,double* out) {
    double t[3]{};for(int a=0;a<3;++a)for(int b=0;b<3;++b)t[a]+=s[3*a+b]*n[b];
    const double nn=t[0]*n[0]+t[1]*n[1]+t[2]*n[2];
    for(int a=0;a<3;++a)for(int b=0;b<3;++b)out[3*a+b]=s[3*a+b]-n[a]*t[b]-t[a]*n[b]+nn*n[a]*n[b];
}
SEIS_POINT void project(Fields w,const Mesh& g,int i,int j) {
    const auto q=index(g,i,j,ghostWidth3D);double s[9],out[9];stress(w,q,s);tangentStress(s,g.normals+3*q,out);storeStress(w,q,out);
}
SEIS_POINT void surfaceDerivative(const Fields& w,const Mesh& g,int i,int j,double* result,int dirX,int dirY) {
    const auto q=index(g,i,j,ghostWidth3D);const double* m=g.metrics+9*q;
    const double lambda=localLambda(g,q),mu=localMu(g,q);
    const double c[3]{m[6],m[7],m[8]};const double c2=c[0]*c[0]+c[1]*c[1]+c[2]*c[2];
    const double length=::sqrt(c2),n[3]{c[0]/length,c[1]/length,c[2]/length};
    double gradient[9]{},traction[3]{};
    for(int v=0;v<3;++v) {
        const double dx=derivative(w.f[v],g,q,0,dirX),dy=derivative(w.f[v],g,q,1,dirY);
        for(int a=0;a<3;++a)gradient[3*v+a]=m[a]*dx+m[3+a]*dy;
    }
    const double trace=gradient[0]+gradient[4]+gradient[8];
    for(int a=0;a<3;++a)for(int b=0;b<3;++b)traction[a]+=((a==b?lambda*trace:0)+mu*(gradient[3*a+b]+gradient[3*b+a]))*c[b];
    const double normal=traction[0]*n[0]+traction[1]*n[1]+traction[2]*n[2];
    for(int a=0;a<3;++a)result[a]=-(traction[a]-n[a]*normal)/(mu*c2)-n[a]*normal/((lambda+2*mu)*c2);
}
SEIS_POINT double tractionFlux(const Fields& w,const Mesh& g,std::size_t q,int axis,int component) {
    const int fields[3][3]{{3,6,7},{6,4,8},{7,8,5}};
    const double* m=g.metrics+9*q+3*axis;double value=0;
    for(int a=0;a<3;++a)value+=m[a]*w.f[fields[component][a]][q];
    return g.jacobian[q]*value;
}
SEIS_POINT double surfaceMomentum(const Fields& w,const Mesh& g,std::size_t q,int component,const int* dirs,int depth) {
    // CGFD3D traction image: reflect J*(grad zeta).sigma, NOT the individual
    // Cartesian stress components using the surface normal at another depth.
    double sum=0;
    for(int axis=0;axis<3;++axis) {
        int offsets[5];double coefficients[5];const int n=stencil(dirs[axis],-1,offsets,coefficients);
        const std::ptrdiff_t stride=axis==0?1:axis==1?g.nx:static_cast<std::ptrdiff_t>(g.nx)*g.ny;
        for(int l=0;l<n;++l) {
            int offset=offsets[l];double sign=1;
            if(axis==2) {
                const int z=depth+offset;
                if(z==0)continue; // Exact zero surface traction (no applied surface force).
                if(z<0){offset=-z-depth;sign=-1;}
            }
            sum+=sign*coefficients[l]*tractionFlux(w,g,static_cast<std::ptrdiff_t>(q)+offset*stride,axis,component);
        }
    }
    return sum/(g.h*g.jacobian[q]*localDensity(g,q));
}
SEIS_POINT void rhs(const Fields& w,Fields out,const Mesh& g,std::size_t q,double source,int step,int stage) {
    const double lambda=localLambda(g,q),mu=localMu(g,q),density=localDensity(g,q);
    double grad[9][3]{};const double* m=g.metrics+9*q;
    const int depth=static_cast<int>(q/(static_cast<std::size_t>(g.nx)*g.ny))-ghostWidth3D;
    const int dirs[3]{direction(step,stage,0),direction(step,stage,1),direction(step,stage,2)};
    double dzSurface[3]{};
    if(depth==0)surfaceDerivative(w,g,static_cast<int>(q%g.nx),static_cast<int>((q/g.nx)%g.ny),dzSurface,dirs[0],dirs[1]);
    for(int f=0;f<9;++f) {
        double d[3];for(int a=0;a<3;++a)d[a]=f<3&&a==2&&depth==0?dzSurface[f]:derivative(w.f[f],g,q,a,dirs[a],f<3&&a==2?depth:-1);
        for(int a=0;a<3;++a)grad[f][a]=m[a]*d[0]+m[3+a]*d[1]+m[6+a]*d[2];
    }
    out.f[0][q]=(grad[3][0]+grad[6][1]+grad[7][2])/density;
    out.f[1][q]=(grad[6][0]+grad[4][1]+grad[8][2])/density;
    out.f[2][q]=(grad[7][0]+grad[8][1]+grad[5][2])/density;
    if(depth<=(dirs[2]?3:1))for(int a=0;a<3;++a)out.f[a][q]=surfaceMomentum(w,g,q,a,dirs,depth);
    for(int a=0;a<3;++a)out.f[a][q]+=source*(g.sourceGradient[a]?g.sourceGradient[a][q]:g.source[q]*g.sourceDirection[a])*(g.density/density);
    const double div=grad[0][0]+grad[1][1]+grad[2][2];
    out.f[3][q]=lambda*div+2*mu*grad[0][0];out.f[4][q]=lambda*div+2*mu*grad[1][1];out.f[5][q]=lambda*div+2*mu*grad[2][2];
    out.f[6][q]=mu*(grad[0][1]+grad[1][0]);out.f[7][q]=mu*(grad[0][2]+grad[2][0]);out.f[8][q]=mu*(grad[1][2]+grad[2][1]);
    for(int f=0;f<9;++f)out.f[f][q]-=g.damping[q]*w.f[f][q];
}
SEIS_POINT double energyDensity(const Fields& w,const Mesh& g,std::size_t q) {
    const double lambda=localLambda(g,q),mu=localMu(g,q),density=localDensity(g,q);
    const double mean=(w.f[3][q]+w.f[4][q]+w.f[5][q])/3;
    double v2=0,dev=0;for(int c=0;c<3;++c){v2+=w.f[c][q]*w.f[c][q];const double t=w.f[c+3][q]-mean;dev+=t*t;}
    for(int c=6;c<9;++c)dev+=2*w.f[c][q]*w.f[c][q];
    const double bulk=lambda+2*mu/3;
    return (0.5*density*v2+dev/(4*mu)+mean*mean/(2*bulk))*g.jacobian[q]*g.h*g.h*g.h;
}
}
#undef SEIS_POINT
