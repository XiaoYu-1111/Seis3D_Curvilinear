#include "PointKernels3D.hpp"
#ifdef _MSC_VER
#pragma warning(push,0) // Preserve reference C bodies; their VLA/int conversions are not production code.
#endif
#include "CGFD3DReference.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace Seis3D::Curvilinear;
// Compare all nine interior rates to the ACTUAL extracted upstream C function,
// using all eight direction tuples and all four stages, in heterogeneous media.
int main() {
    constexpr int n=18;const int count=n*n*n;Point::Mesh mesh;mesh.nx=mesh.ny=mesh.nz=n;mesh.count=count;mesh.h=1;
    std::array<std::vector<float>,9> wf,mf,reference;
    std::array<std::vector<double>,9> wd,computed;
    std::vector<double> metric(9*count),zero(count),lambda(count),mu(count),rho(count);
    std::vector<float> lf(count),uf(count),rf(count);
    for(int f=0;f<9;++f){wf[f].resize(count);wd[f].resize(count);computed[f].resize(count);reference[f].resize(count);mf[f].resize(count);}
    Point::Fields w,out;
    for(int q=0;q<count;++q) {
        lf[q]=2+.2f*std::sin(.09f*q);uf[q]=1+.1f*std::cos(.07f*q);rf[q]=.4f+.02f*std::sin(.03f*q);
        lambda[q]=lf[q];mu[q]=uf[q];rho[q]=1.0/rf[q];
        for(int f=0;f<9;++f){wf[f][q]=std::sin(.11f*q+f)+.3f*std::cos(.037f*q-2*f);wd[f][q]=wf[f][q];mf[f][q]=(f%4==0?1.0f:0.02f)* (1+.1f*std::sin(.013f*q+f));metric[9*q+f]=mf[f][q];}
    }
    for(int f=0;f<9;++f){w.f[f]=wd[f].data();out.f[f]=computed[f].data();}
    mesh.metrics=metric.data();mesh.materialLambda=lambda.data();mesh.materialMu=mu.data();mesh.materialDensity=rho.data();mesh.source=mesh.damping=zero.data();
    double maxRelative=0;
    for(int step=0;step<8;++step)for(int stage=0;stage<4;++stage) {
        int offsets[3][5];float coefs[3][5];
        // Independent literal tables from fd_t.c; do not reuse Point::stencil.
        const int flags[8][3]{{0,0,0},{1,1,0},{1,1,1},{0,0,1},{0,1,0},{1,0,0},{1,0,1},{0,1,1}};
        const int shift[2][5]{{-1,0,1,2,3},{-3,-2,-1,0,1}};
        const float coefficients[2][5]{{-.30874f,-.6326f,1.233f,-.3334f,.04168f},{-.04168f,.3334f,-1.233f,.6326f,.30874f}};
        for(int a=0;a<3;++a){const int d=(flags[step][a]+stage)%2;for(int l=0;l<5;++l){offsets[a][l]=shift[d][l];coefs[a][l]=coefficients[d][l];}}
        Upstream::sv_curv_col_el_iso_rhs_inner(wf[0].data(),wf[1].data(),wf[2].data(),wf[3].data(),wf[4].data(),wf[5].data(),wf[7].data(),wf[8].data(),wf[6].data(),
            reference[0].data(),reference[1].data(),reference[2].data(),reference[3].data(),reference[4].data(),reference[5].data(),reference[7].data(),reference[8].data(),reference[6].data(),
            mf[0].data(),mf[1].data(),mf[2].data(),mf[3].data(),mf[4].data(),mf[5].data(),mf[6].data(),mf[7].data(),mf[8].data(),lf.data(),uf.data(),rf.data(),
            4,n-5,4,n-5,7,n-5,n,n*n,5,offsets[0],coefs[0],5,offsets[1],coefs[1],5,offsets[2],coefs[2],0,0);
        double peak=0,diff=0;
        for(int k=7;k<n-4;++k)for(int j=4;j<n-4;++j)for(int i=4;i<n-4;++i){const auto q=Point::index(mesh,i,j,k);Point::rhs(w,out,mesh,q,0,step,stage);for(int f=0;f<9;++f){peak=std::max(peak,std::abs(double(reference[f][q])));diff=std::max(diff,std::abs(reference[f][q]-computed[f][q]));}}
        maxRelative=std::max(maxRelative,diff/peak);
    }
    std::cout<<"Extracted upstream interior: 32 stage/direction combinations, 9 fields, variable metrics/materials; max relative FP32/FP64 difference="<<maxRelative<<'\n';
    if(maxRelative>=2e-6)return 1;
    // Reflect the computational Z axis: upstream free surface is nk2; ours is k=ghost.
    // Physical Cartesian components stay the same, so only grad(zeta) changes sign.
    std::array<std::vector<float>,9> wu,mt;
    for(int f=0;f<9;++f){wu[f].resize(count);mt[f].resize(count);}
    std::vector<double> jac(count),normals(3*count);
    std::vector<float> ju(count),ru(count),lu(count),uu(count),z2(n*n),mx(9*n*n),my(9*n*n),mz(9*n*n);
    for(int q=0;q<count;++q){jac[q]=static_cast<float>(1+.1*std::sin(.017*q));const double* c=metric.data()+9*q+6;const double length=std::sqrt(c[0]*c[0]+c[1]*c[1]+c[2]*c[2]);for(int a=0;a<3;++a)normals[3*q+a]=c[a]/length;}
    mesh.jacobian=jac.data();mesh.normals=normals.data();
    for(int j=0;j<n;++j)for(int i=0;i<n;++i)Point::project(w,mesh,i,j);
    for(int k0=0;k0<n;++k0)for(int j=0;j<n;++j)for(int i=0;i<n;++i){const auto q=Point::index(mesh,i,j,k0),u=Point::index(mesh,i,j,n-1-k0);ju[u]=static_cast<float>(jac[q]);ru[u]=static_cast<float>(1/rho[q]);lu[u]=static_cast<float>(lambda[q]);uu[u]=static_cast<float>(mu[q]);for(int f=0;f<9;++f){wu[f][u]=static_cast<float>(wd[f][q]);mt[f][u]=static_cast<float>(metric[9*q+f]*(f>=6?-1:1));}}
    // Independently invert the full 3x3 traction-rate matrix, instead of using
    // the production normal/tangential eigenvalue formula.
    for(int j=0;j<n;++j)for(int i=0;i<n;++i) {
        const auto q=Point::index(mesh,i,j,ghostWidth3D);const double* m=metric.data()+9*q;const double c[3]{-m[6],-m[7],-m[8]};const double c2=c[0]*c[0]+c[1]*c[1]+c[2]*c[2];
        for(int axis=0;axis<2;++axis)for(int v=0;v<3;++v) {
            double a[3][4]{};double dot=0;for(int s=0;s<3;++s)dot+=m[3*axis+s]*c[s];
            for(int row=0;row<3;++row){for(int col=0;col<3;++col)a[row][col]=(row==col?mu[q]*c2:0)+(lambda[q]+mu[q])*c[row]*c[col];a[row][3]=-lambda[q]*m[3*axis+v]*c[row]-mu[q]*((row==v?dot:0)+m[3*axis+row]*c[v]);}
            for(int pivot=0;pivot<3;++pivot){const double diagonal=a[pivot][pivot];for(int col=pivot;col<4;++col)a[pivot][col]/=diagonal;for(int row=0;row<3;++row)if(row!=pivot){const double factor=a[row][pivot];for(int col=pivot;col<4;++col)a[row][col]-=factor*a[pivot][col];}}
            for(int row=0;row<3;++row)(axis==0?mx:my)[9*(i+j*n)+3*row+v]=static_cast<float>(a[row][3]);
        }
    }
    double surfaceVelocityDiff=0,surfaceStressDiff=0;int momentumCases=0;
    const int top=n-ghostWidth3D-1;
    for(int step=0;step<8;++step)for(int stage=0;stage<4;++stage) {
        int offsets[3][5];float coefs[3][5];int dirs[3];
        for(int axis=0;axis<3;++axis){dirs[axis]=Point::direction(step,stage,axis);double c[5];Point::stencil(axis==2?1-dirs[axis]:dirs[axis],-1,offsets[axis],c);for(int l=0;l<5;++l)coefs[axis][l]=static_cast<float>(c[l]);for(int a=0;a<5;++a)for(int b=a+1;b<5;++b)if(offsets[axis][a]>offsets[axis][b]){std::swap(offsets[axis][a],offsets[axis][b]);std::swap(coefs[axis][a],coefs[axis][b]);}}
        if(dirs[2]==0) { // Upstream BACKWARD stencil at its upper surface is memory-safe.
            // Its forward stencil reads veczt[-1/-2] at the surface; do not execute UB.
            Upstream::sv_curv_col_el_rhs_timg_z2(wu[3].data(),wu[4].data(),wu[5].data(),wu[7].data(),wu[8].data(),wu[6].data(),reference[0].data(),reference[1].data(),reference[2].data(),
                mt[0].data(),mt[1].data(),mt[2].data(),mt[3].data(),mt[4].data(),mt[5].data(),mt[6].data(),mt[7].data(),mt[8].data(),ju.data(),ru.data(),z2.data(),z2.data(),z2.data(),
                4,n-5,4,n-5,ghostWidth3D,top,n,n*n,5,offsets[0],coefs[0],5,offsets[1],coefs[1],5,offsets[2],coefs[2],0,0);
            double peak=0,diff=0;
            for(int depth=0;depth<=1;++depth)for(int j=4;j<n-4;++j)for(int i=4;i<n-4;++i){const auto q=Point::index(mesh,i,j,ghostWidth3D+depth),u=Point::index(mesh,i,j,top-depth);Point::rhs(w,out,mesh,q,0,step,stage);for(int f=0;f<3;++f){peak=std::max(peak,std::abs(double(reference[f][u])));diff=std::max(diff,std::abs(reference[f][u]-computed[f][q]));}}
            surfaceVelocityDiff=std::max(surfaceVelocityDiff,diff/peak);++momentumCases;
        }
        int zo[4][5];float zc[4][5];Upstream::fd_op_t ops[4];
        for(int depth=0;depth<4;++depth){double c[5];const int len=Point::stencil(1-dirs[2],depth==0?1:depth,zo[depth],c);for(int l=0;l<len;++l)zc[depth][l]=static_cast<float>(c[l]);ops[depth]={len,zo[depth],zc[depth]};}
        Upstream::sv_curv_col_el_iso_rhs_vlow_z2(wu[0].data(),wu[1].data(),wu[2].data(),reference[3].data(),reference[4].data(),reference[5].data(),reference[7].data(),reference[8].data(),reference[6].data(),
            mt[0].data(),mt[1].data(),mt[2].data(),mt[3].data(),mt[4].data(),mt[5].data(),mt[6].data(),mt[7].data(),mt[8].data(),lu.data(),uu.data(),ru.data(),mx.data(),my.data(),mz.data(),z2.data(),z2.data(),z2.data(),
            4,n-5,4,n-5,ghostWidth3D,top,n,n*n,5,offsets[0],coefs[0],5,offsets[1],coefs[1],4,ops,5,0,0);
        double peak=0,diff=0;
        for(int depth=0;depth<3;++depth)for(int j=4;j<n-4;++j)for(int i=4;i<n-4;++i){const auto q=Point::index(mesh,i,j,ghostWidth3D+depth),u=Point::index(mesh,i,j,top-depth);Point::rhs(w,out,mesh,q,0,step,stage);for(int f=3;f<9;++f){peak=std::max(peak,std::abs(double(reference[f][u])));diff=std::max(diff,std::abs(reference[f][u]-computed[f][q]));}}
        surfaceStressDiff=std::max(surfaceStressDiff,diff/peak);
    }
    std::cout<<"Extracted upstream surface: stress rates (32 combinations)="<<surfaceStressDiff<<"; safe momentum stencils ("<<momentumCases<<")="<<surfaceVelocityDiff<<'\n';
    return surfaceVelocityDiff<2e-6&&surfaceStressDiff<2e-6?0:1;
}
