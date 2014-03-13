args <- commandArgs(TRUE)

app <- args[1]
title <- sprintf("average footprint curves of %s", app)
rawdata <- read.table(sprintf("%s.dump", app), skip=1)
legend <- c("cache block size is 64 bytes", "cache block size is 4 bytes")
ltys <- c(1, 2)
cols <- c("red", "blue")

#draw footprint curve
saveto <- sprintf("%s.fp.pdf", app)
x <- rawdata[,2]
y <- rawdata[,3]
y2 <- rawdata[,6]
xlab <- "window size"
ylab <- "footprint"
pdf(file=saveto, width=6, height=4.5)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab, frame.plot=TRUE, col="red", main=title)
legend("topleft", legend, lty=ltys, col=cols)
lines(x, y2, type="l",lty=2, col="blue")
dev.off()

#draw lifetime curve
title <- sprintf("lifetime curves of %s", app)
saveto <- sprintf("%s.lf.pdf", app)
x <- rawdata[,3]
y <- rawdata[,2]
x2 <- rawdata[,6]
xlab <- "cache size"
ylab <- "lifetime"
pdf(file=saveto, width=6, height=4.5)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab, frame.plot=TRUE, col="red", main=title)
legend("topleft", legend, lty=ltys, col=cols)
lines(x2, y, type="l",lty=2, col="blue")
dev.off()


#draw reuse signature
title <- sprintf("reuse signature of %s", app)
saveto <- sprintf("%s.rd.pdf", app)
x <- rawdata[,3]
y <- rawdata[,4]
x2 <- rawdata[,6]
y2 <- rawdata[,7]
xlab <- "reuse distance"
ylab <- "percentage"
pdf(file=saveto, width=6, height=4.5)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab, frame.plot=TRUE, col="red", main=title)
legend("topright", legend, lty=ltys, col=cols)
lines(x2, y2, type="l",lty=2, col="blue")
dev.off()


#draw miss ratio curve
title <- sprintf("miss ratio curves of %s", app)
saveto <- sprintf("%s.mr.pdf", app)
x <- rawdata[,3]
y <- rawdata[,5]
x2 <- rawdata[,6]
y2 <- rawdata[,8]
xlab <- "cache size"
ylab <- "miss ratio"
pdf(file=saveto, width=6, height=4.5)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab, frame.plot=TRUE, col="red", main=title)
legend("topright", legend, lty=ltys, col=cols)
lines(x2, y2, type="l",lty=2, col="blue")
dev.off()
