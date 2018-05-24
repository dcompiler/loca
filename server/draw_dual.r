args <- commandArgs(TRUE)

app <- args[1]
ext <- args[2]
write <- args[3]
title <- sprintf("average footprint curves of %s", app)
rawdata <- read.table(sprintf("%s%s", app,ext), skip=1)
legend <- c("cache block size is 64 bytes", "cache block size is 4 bytes")
ltys <- c(1, 2)
cols <- c("red", "blue")

#draw footprint curve
saveto <- sprintf("%s.fp.png", write)
x <- rawdata[,2]
y <- rawdata[,3]
y2 <- rawdata[,6]
xlab <- "window size"
ylab <- "footprint"
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab, frame.plot=TRUE, col="red", main=title)
legend("topleft", legend, lty=ltys, col=cols)
lines(x, y2, type="l",lty=2, col="blue")
dev.off()

#draw lifetime curve
title <- sprintf("lifetime curves of %s", write)
saveto <- sprintf("%s.lf.png", write)
x <- rawdata[,3]
y <- rawdata[,2]
x2 <- rawdata[,6]
xlab <- "cache size"
ylab <- "lifetime"
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab, frame.plot=TRUE, col="red", main=title)
legend("topleft", legend, lty=ltys, col=cols)
lines(x2, y, type="l",lty=2, col="blue")
dev.off()


#draw reuse signature
title <- sprintf("reuse signature of %s", write)
saveto <- sprintf("%s.rd.png", write)
x <- rawdata[,3]
y <- rawdata[,4]
x2 <- rawdata[,6]
y2 <- rawdata[,7]
xlab <- "reuse distance"
ylab <- "percentage"
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab, frame.plot=TRUE, col="red", main=title)
legend("topright", legend, lty=ltys, col=cols)
lines(x2, y2, type="l",lty=2, col="blue")
dev.off()


#draw miss ratio curve
title <- sprintf("miss ratio curves of %s", write)
saveto <- sprintf("%s.mr.png", write)
x <- rawdata[,3]
y <- rawdata[,5]
x2 <- rawdata[,6]
y2 <- rawdata[,8]
xlab <- "cache size"
ylab <- "miss ratio"
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab, frame.plot=TRUE, col="red", main=title)
legend("topright", legend, lty=ltys, col=cols)
lines(x2, y2, type="l",lty=2, col="blue")
dev.off()
