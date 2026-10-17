"""Losslessly prepare native screenshots and package Markdown for local/Jekyll/Hexo.

All generated files stay in outer out/. No deployment or user site modification.
"""
import csv
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT.parent / 'out'
SHOTS = ROOT / 'docs/images'
DOCS = OUTPUT / 'documentation/Seis3D_Curvilinear'
BLOG = OUTPUT / 'blog/Seis3D_Curvilinear'
TOKEN = re.compile(r'(!?)\[([^\]]*)\]\(([^)]+)\)')
TITLE = 'SeisWave Lab：曲网格二维与三维弹性波模拟的理论、实现与应用'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def archive(folder, destination, files):
    """Publish only explicitly selected assets, never stale generated reports."""
    with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED) as z:
        for relative in sorted(set(files)):
            p=(folder/relative).resolve()
            p.relative_to(folder.resolve())
            assert p.is_file(),p
            z.write(p,relative)

def retain_legacy_reports(folder):
    """Move known former publication intermediates to a separate local archive."""
    out=OUTPUT.resolve()
    for name in ['capture-manifest.json','phase_comparison.csv','summary.json',
                 'VERIFICATION_0_16.md','reference','data','package-validation.json']:
        source=(folder/name).resolve()
        source.relative_to(out)
        if not source.exists(): continue
        target=(out/'documentation-history/2026-10-17'/source.relative_to(out)).resolve()
        target.relative_to(out)
        if target.exists():
            number=1
            while target.with_name(target.name+f'.{number}').exists(): number+=1
            target=target.with_name(target.name+f'.{number}')
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.move(str(source),str(target))

def main():
    github=BLOG/'github-pages';hexo=BLOG/'hexo'
    github_assets=github/'assets/seis3d-curvilinear'
    hexo_post=hexo/'source/_posts/seis3d-curvilinear'
    for folder in [DOCS,github_assets,hexo_post,BLOG]: retain_legacy_reports(folder)
    DOCS.mkdir(parents=True,exist_ok=True)
    (DOCS/'images').mkdir(exist_ok=True)
    rows=list(csv.DictReader((SHOTS/'capture.tsv').open(encoding='utf-8'),delimiter='\t'))
    assert len(rows)==18, 'Expected the complete eighteen-shot workflow'
    for row in rows:
        assert int(row['samples'])==int(row['step'])+1,'Receiver sample count differs from accepted-step timeline'
        assert abs(float(row['time_s'])-int(row['step'])*float(row['dt_s']))<1e-12,'Capture time does not match actual dt'
        png=SHOTS/(row['image']+'.png')
        with Image.open(png) as im:
            assert im.size==(2560,1494), (png,im.size)
            pixels=im.convert('RGB').tobytes()
        row.update(png=png.name,sha256=digest(png),pixel_sha256=hashlib.sha256(pixels).hexdigest())
        shutil.copy2(png,DOCS/'images'/png.name)
    metadata=dict(version='0.16.0',date='2026-10-16',native_size=[2560,1494],font_px=20,language='zh-CN',theme='Paper Sand',pixel_processing='Versioned publication PNG; no crop, resize or recolor during packaging',captures=rows)
    (DOCS.parent/'capture-manifest.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2),encoding='utf-8')
    shutil.copy2(SHOTS/'dispersion-comparison.png',DOCS/'images/dispersion-comparison.png')

    def convert(text,source):
        def link(match):
            bang,label,target=match.groups()
            if target.startswith(('https:','http:','#','mailto:')): return match.group()
            path,_,anchor=target.partition('#');resolved=(source.parent/path).resolve()
            if not resolved.is_file(): raise ValueError(f'Missing local reference: {source}: {target}')
            if resolved.suffix.lower() in ['.png','.jpg','.jpeg']:
                name=resolved.name
                if not (DOCS/'images'/name).is_file(): shutil.copy2(resolved,DOCS/'images'/name)
                dest='images/'+name
            elif resolved.suffix.lower()=='.csv' and resolved.is_relative_to(ROOT/'resources/examples/elevation'):
                dest='examples/elevation/'+resolved.name
                (DOCS/'examples/elevation').mkdir(parents=True,exist_ok=True)
                shutil.copy2(resolved,DOCS/dest)
            else:
                raise ValueError(f'Academic article must be self-contained: {target}')
            if anchor: dest+='#'+anchor
            return f'{bang}[{label}]({dest})'
        return TOKEN.sub(link,text)

    source=ROOT/'docs/PROJECT_MANUAL.md'
    source_text=source.read_text(encoding='utf-8')
    body=convert(source_text,source)
    assert re.findall(r'^## (.+)$',body,re.M)==['1 背景','2 原理','3 实现过程','4 界面介绍','5 附录']
    (DOCS/'PROJECT_MANUAL.md').write_text(body,encoding='utf-8')
    gallery=['# 界面与模拟过程图集','', '18 张 2560×1494 完整界面图，采集日期为 2026 年 10 月 16 日。时间为实际完整步骤对应时刻，AGC/单道视图不改变原始记录。算例物性、震源和边界说明见[主文附录 E](PROJECT_MANUAL.md)。不同会话、自动色标或 AGC 视图不能直接比较绝对幅度；初始界面不构成动态计算精度证据。','']
    image_labels={Path(target).stem:label for bang,label,target in TOKEN.findall(source_text) if bang}
    for number,row in enumerate(rows,1):
        label=image_labels[row['image']]
        gallery += [f"## {number}. {label}",'',f"{row['backend']} | {row['nx']}×{row['ny']}×{row['nz']} | h={row['spacing_m']} m | f₀={row['f0_hz']} Hz | 第 {row['step']} 步 | t={float(row['time_s']):.6f} s | dt={float(row['dt_s'])*1000:.6f} ms | 参考 PPW={float(row['ppw']):.3f} | {row['receivers']} 位置×{row['samples']} 样本",'',f"![{label}](images/{row['png']})",'']
    (DOCS/'SCREENSHOT_GALLERY.md').write_text('\n'.join(gallery),encoding='utf-8')
    for name in ['LICENSE','THIRD_PARTY_NOTICES.md']: shutil.copy2(ROOT/name,DOCS/name)
    (DOCS/'README.md').write_text('# SeisWave Lab 图文说明\n\n[完整说明](PROJECT_MANUAL.md) 按背景、原理、实现过程、界面介绍和附录组织，包含数学推导、算例条件、数据定义与参考文献。[界面与模拟过程图集](SCREENSHOT_GALLERY.md) 收录18张完整界面图，正文另有内点频散分析图，图像均在 images 目录。\n\n本文区分数学模型、当前实现和已有证据范围，不以运行完成或后端一致代替物理精度验证。普通 Markdown 阅读器需启用数学公式渲染。软件许可和来源见随附 LICENSE 与 THIRD_PARTY_NOTICES.md。\n',encoding='utf-8')

    doc_files=['PROJECT_MANUAL.md','SCREENSHOT_GALLERY.md','README.md','LICENSE','THIRD_PARTY_NOTICES.md']
    for resource in sorted((ROOT/'resources/examples/elevation').iterdir()):
        if resource.suffix.lower() not in {'.csv','.md'}: continue
        relative='examples/elevation/'+resource.name
        (DOCS/'examples/elevation').mkdir(parents=True,exist_ok=True)
        shutil.copy2(resource,DOCS/relative)
        doc_files.append(relative)
    doc_files += ['images/'+row['png'] for row in rows]+['images/dispersion-comparison.png']
    # Additional published UI images (e.g. the 2D/3D home screen) are not part
    # of the original 18-shot 3D numerical capture series or its metadata.
    for bang,label,target in TOKEN.findall(body):
        if bang and target.startswith('images/') and target not in doc_files:
            doc_files.append(target)
    (github/'_posts').mkdir(parents=True,exist_ok=True)
    hexo_post.parent.mkdir(parents=True,exist_ok=True)
    for destination in [github_assets,hexo_post]:
        for relative in doc_files:
            target=destination/relative
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(DOCS/relative,target)
    # Bare filenames resolve inside Hexo's same-name post asset folder.
    for relative in doc_files:
        if relative.startswith('images/'):
            shutil.copy2(DOCS/relative,hexo_post/Path(relative).name)
    header='---\ntitle: '+json.dumps(TITLE,ensure_ascii=False)+'\ndate: 2026-10-16\nupdated: 2026-10-17\ntags: [Seismology, Curvilinear Grid, Elastic Wave, CUDA, Finite Difference]\ncategories: [计算地球物理, 数值模拟]\nmathjax: true\ntoc: true\n'
    # No guessed remote URL or domain: relative_url supports project Pages baseurl.
    def github_link(m):
        bang,label,target=m.groups()
        if target.startswith(('http:','https:','#','mailto:')): return m.group()
        return f"{bang}[{label}]({{{{ '/assets/seis3d-curvilinear/{target}' | relative_url }}}})"
    github_body=TOKEN.sub(github_link,body)
    (github/'_posts/2026-10-16-seis3d-curvilinear.md').write_text(header+'layout: post\n---\n\n'+github_body,encoding='utf-8')
    def hexo_link(m):
        bang,label,target=m.groups()
        if target.startswith(('http:','https:','#','mailto:')): return m.group()
        if bang: return f'![{label}]({Path(target).name})'
        return '{% asset_link '+target+' "'+label.replace('"','\\"')+'" %}'
    (hexo/'source/_posts/seis3d-curvilinear.md').write_text(header+'---\n\n'+TOKEN.sub(hexo_link,body),encoding='utf-8')
    (BLOG/'README.md').write_text('''# GitHub Pages / Hexo 发布包

两版均由同一份正文生成，依次为背景、原理、实现过程、界面介绍和附录。数学推导、算例条件、证据边界及参考文献一致，文章修订日期为2026年10月17日。发布资源不包含开发验证日志、构建命令、源码索引或计算中间文件。

## GitHub Pages（Jekyll）

把 github-pages/_posts 的文章合并至站点 _posts，把 assets/seis3d-curvilinear 合并到站点同路径。文章使用 layout: post，站点应有该布局；也可改为现有主题的文章布局。图像使用 Jekyll relative_url 自动考虑 baseurl，无需猜测用户名或仓库名。正文采用 `$...$` 行内公式与 `$$` 独立公式，并使用 aligned 环境和公式编号；请在主题及 Markdown 渲染器中配置对应数学支持。mathjax: true 是主题元数据，不会自行加载脚本。不要把 Jekyll 版直接交给 Hexo 解析。

## Hexo

把 hexo/source/_posts 的文章和同名 seis3d-curvilinear 资源目录合并到站点 source/_posts。正文图片采用 `![说明](01-model.png)`，PNG 直接放在 seis3d-curvilinear 目录中。原 images 子目录仍供附带的离线说明使用，非图像附件链接保留 asset_link。

在 _config.yml 设置 post_asset_folder: true。使用 hexo-renderer-marked 时，同时配置 marked.prependRoot: true 和 marked.postAsset: true，使裸文件名解析到文章同名资源目录，兼顾正文及列表页。该配置依据 https://hexo.io/docs/asset-folders 。公式与 Jekyll 版采用相同行内及独立定界符，请启用支持这些定界符、aligned 环境和 tag 编号的 MathJax/KaTeX 配置及 Markdown 渲染器，避免普通 Markdown 先消耗公式反斜线；mathjax: true 不等于已经安装数学渲染插件。

复制到现有站点即可本地预览；本工具不改站点配置、不覆盖主题，也不执行推送或发布。站点主题的最终数学/页面渲染需在该站点预览。已检查本包 Markdown 引用、图像存在性、PNG 像素摘要和原生分辨率。

## 无站点阅读

assets 下的 PROJECT_MANUAL.md / SCREENSHOT_GALLERY.md 为普通相对路径 Markdown。另有独立 Seis3D_Curvilinear_Documentation.zip 文档包。图片含完整原图；不要对比自动色标的跨时刻绝对幅度。

官方依据：https://jekyllrb.com/docs/front-matter/ ，https://jekyllrb.com/docs/liquid/filters/ ，https://hexo.io/docs/asset-folders 。
''',encoding='utf-8')
    for p in [DOCS/f for f in doc_files if f.endswith('.md')]:
        for bang,label,target in TOKEN.findall(p.read_text(encoding='utf-8')):
            if target.startswith(('https:','http:','#','mailto:')): continue
            assert (p.parent/target.split('#')[0]).is_file(),(p,target)
    assert github_body.count('| relative_url')==len([m for m in TOKEN.finditer(body) if not m[3].startswith(('http:','https:','#','mailto:'))])
    for path in re.findall(r"\{\{ '([^']+)' \| relative_url \}\}",github_body):
        assert (github/path.lstrip('/').split('#')[0]).is_file(),path
    hexo_text=(hexo/'source/_posts/seis3d-curvilinear.md').read_text(encoding='utf-8')
    for path in re.findall(r'\{% asset_(?:img|link) (\S+)',hexo_text):
        assert (hexo_post/path.split('#')[0]).is_file(),path
    for bang,label,path in TOKEN.findall(hexo_text):
        if bang and not path.startswith(('http:','https:')):
            assert Path(path).name==path and (hexo_post/path).is_file(),path
    assert body.count('$$')%2==0 and body.count('```')%2==0,'Unbalanced math/code delimiters'
    report=dict(updated='2026-10-17',sections=5,appendices=8,native_screenshots=len(rows),native_size=[2560,1494],markdown_pages_checked=sum(f.endswith('.md') for f in doc_files),github_article='github-pages/_posts/2026-10-16-seis3d-curvilinear.md',hexo_article='hexo/source/_posts/seis3d-curvilinear.md',hexo_image_format='markdown-bare-filename',pixel_hashes_verified=True)
    report_path=OUTPUT/'maintenance/package-validation.json'
    report_path.parent.mkdir(parents=True,exist_ok=True)
    report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    blog_files=['README.md','github-pages/_posts/2026-10-16-seis3d-curvilinear.md','hexo/source/_posts/seis3d-curvilinear.md']
    blog_files += ['github-pages/assets/seis3d-curvilinear/'+f for f in doc_files]
    blog_files += ['hexo/source/_posts/seis3d-curvilinear/'+f for f in doc_files]
    blog_files += ['hexo/source/_posts/seis3d-curvilinear/'+Path(f).name for f in doc_files if f.startswith('images/')]
    archive(DOCS,OUTPUT/'documentation/Seis3D_Curvilinear_Documentation.zip',doc_files)
    archive(BLOG,OUTPUT/'blog/Seis3D_Curvilinear_GitHub_Pages_Hexo.zip',blog_files)
    for package in [OUTPUT/'documentation/Seis3D_Curvilinear_Documentation.zip',OUTPUT/'blog/Seis3D_Curvilinear_GitHub_Pages_Hexo.zip']:
        with zipfile.ZipFile(package) as z: assert z.testzip() is None,package
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__': main()
