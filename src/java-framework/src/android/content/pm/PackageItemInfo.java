package android.content.pm;
public class PackageItemInfo {
    public String name, packageName;
    public int labelRes, icon, logo, banner;
    public CharSequence nonLocalizedLabel;
    public android.os.Bundle metaData;
    public PackageItemInfo() {}
    public PackageItemInfo(PackageItemInfo o) { name = o.name; packageName = o.packageName; labelRes = o.labelRes; icon = o.icon; metaData = o.metaData; nonLocalizedLabel = o.nonLocalizedLabel; }
    public CharSequence loadLabel(PackageManager pm) {
        if (nonLocalizedLabel != null) return nonLocalizedLabel;
        if (labelRes != 0) { try { return husk.ContextImpl.app().getResources().getText(labelRes); } catch (Exception e) {} }
        return name != null ? name : packageName;
    }
    public android.graphics.drawable.Drawable loadIcon(PackageManager pm) { try { return icon != 0 ? husk.ContextImpl.app().getResources().getDrawable(icon) : null; } catch (Exception e) { return null; } }
    public android.graphics.drawable.Drawable loadUnbadgedIcon(PackageManager pm) { return loadIcon(pm); }
    public android.graphics.drawable.Drawable loadLogo(PackageManager pm) { return null; }
    public android.content.res.XmlResourceParser loadXmlMetaData(PackageManager pm, String name) {
        if (metaData == null) return null;
        int id = metaData.getInt(name);
        return id == 0 ? null : husk.ContextImpl.app().getResources().getXml(id);
    }
}
